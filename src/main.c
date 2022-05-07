#include <core.h>
#include <world.h>
#include <cpe.h>
#include <client.h>
#include <command.h>
#include <event.h>
#include <block.h>
#include <assoc.h>
#include <plugin.h>

typedef struct _SelectionInfo {
	CPECuboid *cub;
	SVec pos[2];
} SelectionInfo;

static SVec InvalidVector = {-1, -1, -1};
static Color4 DefaultSelectionColor = {20, 200, 20, 100};
static AssocType WeAT = ASSOC_INVALID_TYPE;

static SelectionInfo *GetSelection(Client *client) {
	return Assoc_GetPtr(client, WeAT);
}

static void clickhandler(void *param) {
	onPlayerClick *a = (onPlayerClick *)param;
	if(Client_GetHeldBlock(a->client) != BLOCK_AIR || a->button == 0 || a->action == 1)
		return;

	SelectionInfo *sel = GetSelection(a->client);
	if(!sel) return;

	if(Vec_IsNegative(a->tgpos)) {
		if(!Vec_IsNegative(sel->pos[0]) && a->button == 2) {
			sel->pos[0] = InvalidVector;
			Cuboid_SetPositions(sel->cub, InvalidVector, InvalidVector);
			Client_UpdateSelection(a->client, sel->cub);
			Client_Chat(a->client, MESSAGE_TYPE_CHAT, "&dSelection cleared");
		}
	} else if(a->button == 1) {
		if(Vec_IsNegative(sel->pos[0])) {
			sel->pos[0] = a->tgpos;
			Client_Chat(a->client, MESSAGE_TYPE_CHAT, "&dFirst point selected");
		} else if(!SVec_Compare(&sel->pos[0], &a->tgpos) && !SVec_Compare(&sel->pos[1], &a->tgpos)) {
			sel->pos[1] = a->tgpos;
			Cuboid_SetPositions(sel->cub, sel->pos[0], sel->pos[1]);
			Client_UpdateSelection(a->client, sel->cub);
			Client_Chat(a->client, MESSAGE_TYPE_CHAT, "&dSecond point selected");
		}
	}
}

static cs_int32 checkside(cs_char *str) {
	if(String_CaselessCompare(str, "forward") || String_CaselessCompare(str, "front"))
		return 0;
	else if(String_CaselessCompare(str, "left"))
		return 1;
	else if(String_CaselessCompare(str, "backward") || String_CaselessCompare(str, "back"))
		return 2;
	else if(String_CaselessCompare(str, "right"))
		return 3;
	return -1;
}

static void norinc(cs_int16 *v1, cs_int16 *v2, cs_int16 cnt, cs_int16 maxv) {
	if(*v1 > *v2) *v1 = min(*v1 + cnt, maxv);
	else *v2 = min(*v2 + cnt, maxv);
}

static void nordec(cs_int16 *v1, cs_int16 *v2, cs_int16 cnt, cs_int16 minv) {
	if(*v1 < *v2) *v1 = max(*v1 - cnt, minv);
	else *v2 = max(*v2 - cnt, minv);
}

COMMAND_FUNC(Expand) {
	COMMAND_SETUSAGE("/expand <count> <side>");
	cs_char arg1[12], arg2[12];

	SelectionInfo *sel = GetSelection(ccdata->caller);
	if(!sel) COMMAND_PRINT("Select cuboid first");

	if(COMMAND_GETARG(arg1, 12, 0) && COMMAND_GETARG(arg2, 12, 1)) {
		cs_int32 cnt = String_ToInt(arg1);
		cs_char *side = NULL;
		if(cnt != 0) {
			side = arg2;
		} else {
			side = arg1;
			cnt = String_ToInt(arg2);
		}

		SVec dims;
		World *world = Client_GetWorld(ccdata->caller);
		World_GetDimensions(world, &dims);

		if(String_CaselessCompare(side, "up")) {
			norinc(&sel->pos[0].y, &sel->pos[1].y, (cs_int16)cnt, dims.y);
			goto cuboidupdated;
		} else if(String_CaselessCompare(side, "down")) {
			nordec(&sel->pos[0].y, &sel->pos[1].y, (cs_int16)cnt, 0);
			goto cuboidupdated;
		}

		cs_int32 diroffset = -1;
		Ang rot;
		Client_GetPosition(ccdata->caller, NULL, &rot);
		cs_int32 dirplayer = ((cs_int32)(rot.yaw + 315.0f) % 360) / 90;

		switch(checkside(side)) {
			case 0:
				diroffset = (dirplayer + 4) % 4;
				break;
			case 1:
				diroffset = (dirplayer + 3) % 4;
				break;
			case 2:
				diroffset = (dirplayer + 2) % 4;
				break;
			case 3:
				diroffset = (dirplayer + 1) % 4;
				break;
			default:
				COMMAND_PRINTUSAGE;
		}

		if(diroffset == -1)
			COMMAND_PRINTUSAGE;

		switch(diroffset) {
			case 0: // спереди
				norinc(&sel->pos[0].x, &sel->pos[1].x, (cs_int16)cnt, dims.x);
				break;
			case 1: // справа
				norinc(&sel->pos[0].z, &sel->pos[1].z, (cs_int16)cnt, dims.z);
				break;
			case 2: // сзади
				nordec(&sel->pos[0].x, &sel->pos[1].x, (cs_int16)cnt, 0);
				break;
			case 3: // слева
				nordec(&sel->pos[0].z, &sel->pos[1].z, (cs_int16)cnt, 0);
				break;
		}

		cuboidupdated:
		Cuboid_SetPositions(sel->cub, sel->pos[0], sel->pos[1]);
		Client_UpdateSelection(ccdata->caller, sel->cub);
		COMMAND_PRINTF("&dExpanded for %d blocks %s", cnt, side);
	}

	COMMAND_PRINTUSAGE;
}

COMMAND_FUNC(Select) {
	SelectionInfo *sel = GetSelection(ccdata->caller);
	if(sel) {
		Client_RemoveSelection(ccdata->caller, sel->cub);
		Assoc_Remove(ccdata->caller, WeAT);
		COMMAND_PRINT("Selection mode &cdisabled");
	}
	sel = Assoc_AllocFor(ccdata->caller, WeAT, 1, sizeof(SelectionInfo));
	sel->cub = Client_NewSelection(ccdata->caller);
	Vec_Set(sel->pos[0], -1, -1, -1); Vec_Set(sel->pos[1], -1, -1, -1);
	Cuboid_SetColor(sel->cub, DefaultSelectionColor);
	COMMAND_PRINT("Selection mode &aenabled");
}

COMMAND_FUNC(Set) {
	COMMAND_SETUSAGE("/set <blockid>");
	SelectionInfo *sel = GetSelection(ccdata->caller);
	if(!sel) COMMAND_PRINT("Select cuboid first");

	char blid[4];
	if(!COMMAND_GETARG(blid, 4, 0))
		COMMAND_PRINTUSAGE;

	BlockID block = (BlockID)String_ToInt(blid);
	World *world = Client_GetWorld(ccdata->caller);
	if(!Block_IsValid(world, block))
		COMMAND_PRINT("Unknown block id");

	SVec s = {0}, e = {0};
	Cuboid_GetPositions(sel->cub, &s, &e);
	cs_uint32 count = Cuboid_GetSize(sel->cub);
	BulkBlockUpdate bbu = {
		.world = world,
		.autosend = true
	};
	Block_BulkUpdateClean(&bbu);

	World_Lock(world, 0);
	for(cs_uint16 x = s.x; x < e.x; x++) {
		for(cs_uint16 y = s.y; y < e.y; y++) {
			for(cs_uint16 z = s.z; z < e.z; z++) {
				SVec pos; Vec_Set(pos, x, y, z);
				cs_uint32 offset = World_GetOffset(world, &pos);
				if(offset != (cs_uint32)-1) {
					Block_BulkUpdateAdd(&bbu, offset, block);
					World_SetBlockO(world, offset, block);
				}
			}
		}
	}
	Block_BulkUpdateSend(&bbu);
	World_Unlock(world);

	const char *to_name = Block_GetName(world, block);
	COMMAND_PRINTF("&d%d blocks filled with %s", count, to_name);
}

COMMAND_FUNC(Replace) {
	COMMAND_SETUSAGE("/repalce <from> <to>");
	SelectionInfo *sel = GetSelection(ccdata->caller);
	if(!sel) COMMAND_PRINT("Select cuboid first");

	char fromt[4], tot[4];
	if(!COMMAND_GETARG(fromt, 4, 0)||
	!COMMAND_GETARG(tot, 4, 1))
		COMMAND_PRINTUSAGE;

	World *world = Client_GetWorld(ccdata->caller);
	BlockID from = (BlockID)String_ToInt(fromt),
	to = (BlockID)String_ToInt(tot);
	if(!Block_IsValid(world, from) || !Block_IsValid(world, to))
		COMMAND_PRINT("Unknown block id");

	BlockID *blocks = World_GetBlockArray(world, NULL);
	SVec s = {0}, e = {0};
	Cuboid_GetPositions(sel->cub, &s, &e);
	cs_uint32 count = 0;

	BulkBlockUpdate bbu = {
		.world = world,
		.autosend = true
	};
	
	World_Lock(world, 0);
	Block_BulkUpdateClean(&bbu);
	for(cs_uint16 x = s.x; x < e.x; x++) {
		for(cs_uint16 y = s.y; y < e.y; y++) {
			for(cs_uint16 z = s.z; z < e.z; z++) {
				SVec pos; Vec_Set(pos, x, y, z);
				cs_int32 offset = World_GetOffset(world, &pos);
				if(offset != -1 && blocks[offset] == from) {
					Block_BulkUpdateAdd(&bbu, offset, to);
					World_SetBlockO(world, offset, to);
					count++;
				}
			}
		}
	}
	Block_BulkUpdateSend(&bbu);
	World_Unlock(world);

	cs_str to_name = Block_GetName(world, to), from_name = Block_GetName(world, from);
	COMMAND_PRINTF("&d%d blocks of %s replaced with %s", count, from_name, to_name);
}

static void freeselvecs(void *param) {
	Assoc_Remove(param, WeAT);
}

Plugin_SetVersion(1);

Event_DeclareBunch (events) {
	EVENT_BUNCH_ADD('v', EVT_ONCLICK, clickhandler)
	EVENT_BUNCH_ADD('v', EVT_ONDISCONNECT, freeselvecs)

	EVENT_BUNCH_END
};

cs_bool Plugin_Load(void) {
	WeAT = Assoc_NewType(ASSOC_BIND_CLIENT);
	Command *cmd;
	cmd = COMMAND_ADD(Select, CMDF_OP | CMDF_CLIENT, "Activates area selection mode");
	Command_SetAlias(cmd, "sel");
	cmd = COMMAND_ADD(Set, CMDF_OP | CMDF_CLIENT, "Fills selected area with specified block");
	Command_SetAlias(cmd, "fill");
	cmd = COMMAND_ADD(Replace, CMDF_OP | CMDF_CLIENT, "Replaces specified block in selected area");
	Command_SetAlias(cmd, "repl");
	cmd = COMMAND_ADD(Expand, CMDF_OP | CMDF_CLIENT, "Expands selected aread");
	Command_SetAlias(cmd, "expd");
	return Event_RegisterBunch(events);
}

cs_bool Plugin_Unload(cs_bool force) {
	(void)force;
	Assoc_DelType(WeAT);
	COMMAND_REMOVE(Select);
	COMMAND_REMOVE(Set);
	COMMAND_REMOVE(Replace);
	COMMAND_REMOVE(Expand);
	Event_UnregisterBunch(events);
	return true;
}
