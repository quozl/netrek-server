#!/usr/bin/python
# log in to a netrek server on the player port and obtain the version
# from the first line of the message of the day.
import sys, socket, struct

# derived from py-struct entries in packets.h
SP_MOTD = (11, "!bxxx80s")
CP_SOCKET = (27, '!bbbxI')
CP_BYE = (29, "!bxxx")

# connect to the server and send a CP_SOCKET packet
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((sys.argv[1], 2592))
s.send(struct.pack(CP_SOCKET[1], CP_SOCKET[0], 4, 10, 0))

# read from network only as many bytes as would be expected for an SP_MOTD
p = s.recv(struct.calcsize(SP_MOTD[1]))

# extract the packet type byte
p_type = struct.unpack('b', p[0])

# check for the expected SP_MOTD type
if p_type[0] == SP_MOTD[0]:
    (ignored, message) = struct.unpack(SP_MOTD[1], p[0:84])
    text = message.split('\000')[0]
    print text

# send a goodbye packet
s.send(struct.pack(CP_BYE[1], CP_BYE[0]))
s.close()
