/* Plugin Template generated by Pawn Studio */

#include <sourcemod>
#include <swarmtools>

public Plugin:myinfo = 
{
	name = "Ch1ckensCoop Client Effects",
	author = "IBeMad",
	description = "Allows clients to easily enable/disable client effects.",
	version = "1.0",
	url = "ch1ckenscoop.googlecode.com"
}

public OnPluginStart()
{
	RegConsoleCmd("say", Say_Called);
}

public Action:Say_Called(client, args)
{
	new String:text[192]
	GetCmdArgString(text, sizeof(text))
 
	new startidx = 0
	if (text[0] == '"')
	{
		startidx = 1
		/* Strip the ending quote, if there is one */
		new len = strlen(text);
		if (text[len-1] == '"')
		{
			text[len-1] = '\0'
		}
	}
 
	if (StrEqual(text[startidx], "/cfx"))
	{
		new String:SteamID[64];
		GetClientAuthString(client, SteamID, sizeof(SteamID));
		if (GetPreviousValue(SteamID))
		{
			ReplyToCommand(client, "Client effects are now disabled.");
			SetNewValue(SteamID, 0);
			SetCFX(false, client);
		} else {
			ReplyToCommand(client, "Client effects are now enabled!");
			SetNewValue(SteamID, 1);
			SetCFX(true, client);
		}
		/* Block the client's messsage from broadcasting */
		return Plugin_Handled;
	}
 
	/* Let say continue normally */
	return Plugin_Continue;
}

GetPreviousValue(String:steamID[])
{
	new Handle:CFX_Prefs = CreateKeyValues("CFX_Settings");
	
	FileToKeyValues(CFX_Prefs, "cfg/CFX/client_effects_prefs.cfg");
	
	KvJumpToKey(CFX_Prefs, steamID, true);
	new value = KvGetNum(CFX_Prefs, "enabled", 1);
	
	CloseHandle(CFX_Prefs);
	
	return value;
}

SetNewValue(String:steamID[], value)
{
	new Handle:CFX_Prefs = CreateKeyValues("CFX_Settings");
	
	FileToKeyValues(CFX_Prefs, "cfg/CFX/client_effects_prefs.cfg");
	
	KvJumpToKey(CFX_Prefs, steamID, true);
	KvSetNum(CFX_Prefs, "enabled", value);
	
	CloseHandle(CFX_Prefs);
}

SetCFX(bool:bEnabled, client)
{
	new String:command[128];
	Format(command, sizeof(command), "cfx_toggle %i %i", client, bEnabled);
	ServerCommand(command);
}