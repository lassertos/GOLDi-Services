#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <systemd/sd-daemon.h>
#include <string.h>
#include <stdio.h>
#include "sockets.h"

char* serializeInt(int num)
{
    char result[4];
    result[0] = num >> 24;
    result[1] = num >> 16;
    result[2] = num >> 8;
    result[3] = num;
    return result;
}

int deserializeInt(char* str)
{
    if (str == NULL)
        return -1;
    int result = str[0];
    result = (result << 8) + str[1];
    result = (result << 8) + str[2];
    result = (result << 8) + str[3];
    return result;
}

/*
 *  Either fetches the FileDescriptor given by systemd socket activation or creates
 *  a new socket for communication. Only needed if the Service needs to accept an
 *  incoming IPC-socket connection. 
 */
int createIPCSocket(char* socketname)
{
    if(sd_listen_fds(0) == 1)
    {
        return SD_LISTEN_FDS_START;
    }
    else
    {
        struct sockaddr_un addr;
        int fd;
        if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
            perror("socket error");
            return -1;
        }

        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;

        *addr.sun_path = '\0';
        strncpy(addr.sun_path+1, socketname, sizeof(addr.sun_path)-2);

        // +3 because of 2 bytes sun_family and 1 byte 0 character
        if (bind(fd, (struct sockaddr*)&addr, 3 + strlen(socketname)) == -1) 
        {
            perror("bind error");
            return -1;
        }

        if (listen(fd, 5) == -1) 
        {
            perror("listen error");
            return -1;
        }

        return fd;
    }
}

/*
 *  Creates a new socket connection to the socket specified by socketname.
 *  Used for a connection request from the client-side. 
 */ 
IPCSocketConnection* connectToIPCSocket(char* socketname)
{
    int fd;
    struct sockaddr_un addr;
    IPCSocketConnection* connection = malloc(sizeof *connection);

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket error");
        free(connection);
		return NULL;
	}

    connection->fd = fd;
    connection->socketname = socketname;
    connection->id = fd;
    connection->buffer = NULL;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    *addr.sun_path = '\0';
    strncpy(addr.sun_path+1, socketname, sizeof(addr.sun_path)-2);

    // +3 because of 2 bytes sun_family and 1 byte 0 character
    if (connect(fd, (struct sockaddr*)&addr, 3 + strlen(socketname)) == -1) {
    	perror("connect error");
        free(connection);
    	return NULL;
    }

    return connection;
}

/*
 *  Accepts an incoming connection request on the socket specified by fd
 *  from the socket specified by socketname.
 */ 
IPCSocketConnection* acceptIPCConnection(int fd, char* socketname)
{
    IPCSocketConnection* connection = malloc(sizeof *connection);
    connection->fd = fd;
    connection->socketname = socketname;
    connection->buffer = NULL;

    if ((connection->id = accept(fd, NULL, NULL)) == -1) 
    {
        free(connection);
        perror("accept error");
        return NULL;
    }

    return connection;
}

/*
 *  Sends a message to the IPC socket specified by ipcsc.
 */
int sendMessageIPC(IPCSocketConnection* ipcsc, int messageType, char* msg)
{
    int messageLength = strlen(msg) + 1;
    int completeLength = messageLength + sizeof(int) * 2;

    // prepare buffer for sending the message 
    ipcsc->buffer = malloc(completeLength);

    // serialize messageType and messageLength
    strncpy(ipcsc->buffer, serializeInt(messageType), 4);
    strncpy(ipcsc->buffer+4, serializeInt(messageLength), 4);

    // copy message into buffer and add 0 character
    strncpy(ipcsc->buffer+8, msg, messageLength-1);
    ipcsc->buffer[completeLength-1] = '\0';

    if (write(socket, ipcsc->buffer, completeLength) != completeLength)
    {
        if (completeLength > 0) fprintf(stderr,"partial write");
        else
        {
            perror("write error");
            free(ipcsc->buffer);
            return -1;
        }
    }

    free(ipcsc->buffer);
    return messageLength;
}

/*
 *  Reads the specified amount of bytes from the given socket. 
 */ 
char* readFromSocket(IPCSocketConnection* ipcsc, int bytes)
{
    int rc;
    char* result = malloc(bytes);
    if (rc = read(ipcsc->id, ipcsc->buffer, bytes) > 0)
    {
        if (rc == bytes)
            return result;
        else 
            return NULL;
    }
    if (rc == 0)
    {
        close(ipcsc->id);
        return NULL;
    }
    else if (rc == -1)
    {
        perror("read");
        return NULL;
    }
}

/*
 *  Receives a message from the IPC socket specified by ipcsc.
 */
Message receiveMessageIPC(IPCSocketConnection* ipcsc)
{
    int rc, type, length;
    Message message;

    message.type = deserializeInt(readFromSocket(ipcsc, sizeof(int)));
    message.length = deserializeInt(readFromSocket(ipcsc, sizeof(int)));
    
    if (message.type == -1 || message.type == -1)
    {   
        message.type = -1;
        message.length = -1;
        message.content = NULL;
        return message;
    }

    message.content = readFromSocket(ipcsc, length);

    return message;
}

/*
 *  Closes an IPC Connection
 */
void closeIPCConnection(IPCSocketConnection* ipcsc)
{
    close(ipcsc->id);
    free(ipcsc);
}