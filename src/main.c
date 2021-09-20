#include <core.h>
#include <client.h>
#include <command.h>
#include <event.h>
#include <block.h>
#include "main.h"

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

static void clickhandler(void *param) {
	onPlayerClick *a = (onPlayerClick *)param;
	if(Client_GetHeldBlock(a->client) != BLOCK_AIR || a->button == 0)
		return;

	SVec *vecs = GetCuboid(a->client);
	if(!vecs) return;

	cs_bool isVecInvalid = Vec_IsInvalid(&a->tgpos);
	if(isVecInvalid && a->button == 2) {
		Vec_Set(vecs[0], -1, -1, -1);
		Client_RemoveSelection(a->client, 0);
	} else if(!isVecInvalid && a->button == 1) {
		if(vecs[0].x == -1)
			vecs[0] = a->tgpos;
		else if(!SVec_Compare(&vecs[0], &a->tgpos) && !SVec_Compare(&vecs[1], &a->tgpos)) {
			vecs[1] = a->tgpos;
			SVec s = vecs[0], e = vecs[1];
			CubeNormalize(&s, &e);
			Client_MakeSelection(a->client, 0, &s, &e, &DefaultSelectionColor);
		}
	}
}

COMMAND_FUNC(Select) {
	SVec *ptr = GetCuboid(ccdata->caller);
	if(ptr) {
		Client_RemoveSelection(ccdata->caller, 0);
		Assoc_Remove(ccdata->caller, WeAT, true);
		COMMAND_PRINT("Selection mode &cdisabled");
	}
	ptr = Memory_Alloc(2, sizeof(SVec));
	Vec_Set(ptr[0], -1, -1, -1);
	Vec_Set(ptr[1], -1, -1, -1);
	Assoc_Set(ccdata->caller, WeAT, ptr);
	COMMAND_PRINT("Selection mode &aenabled");
}

COMMAND_FUNC(Set) {
	COMMAND_SETUSAGE("/set <blockid>");
	Client *client = ccdata->caller;
	SVec *ptr = GetCuboid(client);
	if(!ptr) {
		COMMAND_PRINT("Select cuboid first.");
	}

	char blid[4];
	if(!COMMAND_GETARG(blid, 4, 0)){
		COMMAND_PRINTUSAGE;
	}

	BlockID block = (BlockID)String_ToInt(blid);
	World *world = Client_GetWorld(client);
	SVec s = ptr[0], e = ptr[1];
	CubeNormalize(&s, &e);
	cs_uint32 count = (s.x - e.x) * (s.y - e.y) * (s.z - e.z);
	BulkBlockUpdate bbu;
	Block_BulkUpdateClean(&bbu);
	bbu.world = world;
	bbu.autosend = true;

	for(cs_uint16 x = e.x; x < s.x; x++) {
		for(cs_uint16 y = e.y; y < s.y; y++) {
			for(cs_uint16 z = e.z; z < s.z; z++) {
				SVec pos; Vec_Set(pos, x, y, z);
				cs_int32 offset = World_GetOffset(world, &pos);
				if(offset != -1) {
					Block_BulkUpdateAdd(&bbu, offset, block);
					World_SetBlockO(world, offset, block);
				}
			}
		}
	}

	Block_BulkUpdateSend(&bbu);
	const char *to_name = Block_GetName(block);
	COMMAND_PRINTF("%d blocks filled with %s.", count, to_name);
}

COMMAND_FUNC(Replace) {
	COMMAND_SETUSAGE("/repalce <from> <to>");
	Client *client = ccdata->caller;
	SVec *ptr = GetCuboid(client);
	if(!ptr) {
		COMMAND_PRINT("Select cuboid first.");
	}

	char fromt[4], tot[4];
	if(!COMMAND_GETARG(fromt, 4, 0)||
	!COMMAND_GETARG(tot, 4, 1)) {
		COMMAND_PRINTUSAGE;
	}

	World *world = Client_GetWorld(client);
	BlockID from = (BlockID)String_ToInt(fromt),
	to = (BlockID)String_ToInt(tot),
	*blocks = World_GetBlockArray(world, NULL);
	SVec s = ptr[0], e = ptr[1];
	CubeNormalize(&s, &e);
	cs_uint32 count = 0;

	BulkBlockUpdate bbu;
	Block_BulkUpdateClean(&bbu);
	bbu.world = world;
	bbu.autosend = true;

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
	const char *to_name = Block_GetName(to);
	COMMAND_PRINTF("%d blocks replaced with %s.", count, to_name);
}

static void freeselvecs(void *param) {
	Assoc_Remove((Client *)param, WeAT, true);
}

Plugin_SetVersion(1)

cs_bool Plugin_Load(void) {
	WeAT = Assoc_NewType();
	Command *cmd;
	cmd = COMMAND_ADD(Select, CMDF_OP | CMDF_CLIENT);
	Command_SetAlias(cmd, "sel");
	cmd = COMMAND_ADD(Set, CMDF_OP | CMDF_CLIENT);
	Command_SetAlias(cmd, "fill");
	cmd = COMMAND_ADD(Replace, CMDF_OP | CMDF_CLIENT);
	Command_SetAlias(cmd, "repl");
	Event_RegisterVoid(EVT_ONCLICK, clickhandler);
	Event_RegisterVoid(EVT_ONDISCONNECT, freeselvecs);
	return true;
}

cs_bool Plugin_Unload(cs_bool force) {
	(void)force;
	Assoc_DelType(WeAT, true);
	COMMAND_REMOVE(Select);
	COMMAND_REMOVE(Set);
	COMMAND_REMOVE(Replace);
	EVENT_UNREGISTER(EVT_ONCLICK, clickhandler);
	EVENT_UNREGISTER(EVT_ONDISCONNECT, freeselvecs);
	return true;
}
