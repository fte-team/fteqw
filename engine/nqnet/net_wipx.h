/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// net_wipx.h

int  WIPX_Init (void);
void WIPX_Shutdown (void);
void WIPX_Listen (qboolean state);
int  WIPX_OpenSocket (int port);
int  WIPX_CloseSocket (int socket);
int  WIPX_Connect (int socket, struct sockaddr_qstorage *addr);
int  WIPX_CheckNewConnections (void);
int  WIPX_Read (int socket, qbyte *buf, int len, struct sockaddr_qstorage *addr);
int  WIPX_Write (int socket, qbyte *buf, int len, struct sockaddr_qstorage *addr);
int  WIPX_Broadcast (int socket, qbyte *buf, int len);
char *WIPX_AddrToString (struct sockaddr_qstorage *addr);
int  WIPX_StringToAddr (char *string, struct sockaddr_qstorage *addr);
int  WIPX_GetSocketAddr (int socket, struct sockaddr_qstorage *addr);
int  WIPX_GetNameFromAddr (struct sockaddr_qstorage *addr, char *name);
int  WIPX_GetAddrFromName (char *name, struct sockaddr_qstorage *addr);
int  WIPX_AddrCompare (struct sockaddr_qstorage *addr1, struct sockaddr_qstorage *addr2);
int  WIPX_GetSocketPort (struct sockaddr_qstorage *addr);
int  WIPX_SetSocketPort (struct sockaddr_qstorage *addr, int port);
