#include <core.h>
#include <world.h>
#include <client.h>
#include <command.h>
#include <event.h>
#include <block.h>
#include <assoc.h>
#include <plugin.h>

static Color4 DefaultSelectionColor = {20, 200, 20, 100};
static cs_uint16 WeAT;

static void CubeNormalize(SVec *s, SVec *e) {
	cs_int16 tmp, *a = (cs_int16 *)s, *b = (cs_int16 *)e;
	for(int i = 0; i < 3; i++) {
		if(a[i] < b[i]) {
			tmp = a[i];
			a[i] = b[i];
			b[i] = tmp;
		}
		a[i]++;
	}
}

static SVec *GetCuboid(Client *client) {
	return Assoc_GetPtr(client, WeAT);
}

static void SendCuboid(Client *client, SVec *vecs) {
	SVec s = vecs[0], e = vecs[1];
	CubeNormalize(&s, &e);
	Client_MakeSelection(client, 0, &s, &e, &DefaultSelectionColor);
}

static void clickhandler(void *param) {
	onPlayerClick *a = (onPlayerClick *)param;
	if(Client_GetHeldBlock(a->client) != BLOCK_AIR || a->button == 0 || a->action == 1)
		return;

	SVec *vecs = GetCuboid(a->client);
	if(!vecs) return;

	if(Vec_IsNegative(a->tgpos)) {
		if(!Vec_IsNegative(vecs[0]) && a->button == 2) {
			Vec_Set(vecs[0], -1, -1, -1);
			Client_RemoveSelection(a->client, 0);
			Client_Chat(a->client, MESSAGE_TYPE_CHAT, "&dSelection cleared");
		}
	} else if(a->button == 1) {
		if(Vec_IsNegative(vecs[0])) {
			vecs[0] = a->tgpos;
			Client_Chat(a->client, MESSAGE_TYPE_CHAT, "&dFirst point selected");
		} else if(!SVec_Compare(&vecs[0], &a->tgpos) && !SVec_Compare(&vecs[1], &a->tgpos)) {
			vecs[1] = a->tgpos;
			SendCuboid(a->client, vecs);
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

	SVec *vecs = GetCuboid(ccdata->caller);
	if(!vecs) {
		COMMAND_PRINT("Select cuboid first");
	}

	if(COMMAND_GETARG(arg1, 12, 0) && COMMAND_GETARG(arg2, 12, 1)) {
		cs_int32 cnt = String_ToInt(arg1);
		cs_char *side = NULL;
		if(cnt > 0) {
			side = arg2;
		} else {
			side = arg1;
			cnt = String_ToInt(arg2);
		}

		if(cnt < 1) {
			COMMAND_PRINTUSAGE;
		}

		SVec dims;
		World *world = Client_GetWorld(ccdata->caller);
		World_GetDimensions(world, &dims);

		if(String_CaselessCompare(side, "up")) {
			norinc(&vecs[0].y, &vecs[1].y, (cs_int16)cnt, dims.y);
			goto cuboidupdated;
		} else if(String_CaselessCompare(side, "down")) {
			nordec(&vecs[0].y, &vecs[1].y, (cs_int16)cnt, 0);
			goto cuboidupdated;
		}

		cs_int32 diroffset = -1;
		Ang rot;
		if(!Client_GetPosition(ccdata->caller, NULL, &rot)) {
			COMMAND_PRINT("Internal error");
		}

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

		if(diroffset == -1) {
			COMMAND_PRINTUSAGE;
		}

		switch(diroffset) {
			case 0: // спереди
				norinc(&vecs[0].x, &vecs[1].x, (cs_int16)cnt, dims.x);
				break;
			case 1: // справа
				norinc(&vecs[0].z, &vecs[1].z, (cs_int16)cnt, dims.z);
				break;
			case 2: // сзади
				nordec(&vecs[0].x, &vecs[1].x, (cs_int16)cnt, 0);
				break;
			case 3: // слева
				nordec(&vecs[0].z, &vecs[1].z, (cs_int16)cnt, 0);
				break;
		}

		cuboidupdated:
		SendCuboid(ccdata->caller, vecs);
		COMMAND_PRINTF("&dExpanded for %d blocks %s", cnt, side)
	}

	COMMAND_PRINTUSAGE;
}

COMMAND_FUNC(Select) {
	SVec *ptr = GetCuboid(ccdata->caller);
	if(ptr) {
		Client_RemoveSelection(ccdata->caller, 0);
		Assoc_Remove(ccdata->caller, WeAT);
		COMMAND_PRINT("Selection mode &cdisabled");
	}
	ptr = Assoc_AllocFor(ccdata->caller, WeAT, 2, sizeof(SVec));
	Vec_Set(ptr[0], -1, -1, -1); Vec_Set(ptr[1], -1, -1, -1);
	COMMAND_PRINT("Selection mode &aenabled");
}

COMMAND_FUNC(Set) {
	COMMAND_SETUSAGE("/set <blockid>");
	SVec *ptr = GetCuboid(ccdata->caller);
	if(!ptr) {
		COMMAND_PRINT("Select cuboid first");
	}

	char blid[4];
	if(!COMMAND_GETARG(blid, 4, 0)){
		COMMAND_PRINTUSAGE;
	}

	BlockID block = (BlockID)String_ToInt(blid);
	World *world = Client_GetWorld(ccdata->caller);
	SVec s = ptr[0], e = ptr[1];
	CubeNormalize(&s, &e);
	cs_uint32 count = (s.x - e.x) * (s.y - e.y) * (s.z - e.z);
	BulkBlockUpdate bbu = {
		.world = world,
		.autosend = true
	};
	Block_BulkUpdateClean(&bbu);

	World_Lock(world, 0);
	for(cs_uint16 x = e.x; x < s.x; x++) {
		for(cs_uint16 y = e.y; y < s.y; y++) {
			for(cs_uint16 z = e.z; z < s.z; z++) {
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
	SVec *ptr = GetCuboid(ccdata->caller);
	if(!ptr) {
		COMMAND_PRINT("Select cuboid first");
	}

	char fromt[4], tot[4];
	if(!COMMAND_GETARG(fromt, 4, 0)||
	!COMMAND_GETARG(tot, 4, 1)) {
		COMMAND_PRINTUSAGE;
	}

	World *world = Client_GetWorld(ccdata->caller);
	BlockID from = (BlockID)String_ToInt(fromt),
	to = (BlockID)String_ToInt(tot),
	*blocks = World_GetBlockArray(world, NULL);
	SVec s = ptr[0], e = ptr[1];
	CubeNormalize(&s, &e);
	cs_uint32 count = 0;

	BulkBlockUpdate bbu = {
		.world = world,
		.autosend = true
	};
	
	World_Lock(world, 0);
	Block_BulkUpdateClean(&bbu);
	for(cs_uint16 x = e.x; x < s.x; x++) {
		for(cs_uint16 y = e.y; y < s.y; y++) {
			for(cs_uint16 z = e.z; z < s.z; z++) {
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

	const char *to_name = Block_GetName(world, to);
	COMMAND_PRINTF("&d%d blocks replaced with %s", count, to_name);
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
