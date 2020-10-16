AUTOMAKE_OPTIONS = subdir-objects

IPCSockets = interfaces/ipcsockets.h interfaces/ipcsockets.c
WebSockets = interfaces/websockets.h interfaces/websockets.c
Utils = utils/utils.h utils/utils.c
SPI = interfaces/spi.h interfaces/spi.c
Stack = utils/stack.h utils/stack.c
Queue = utils/queue.h utils/queue.c

bin_PROGRAMS = CommunicationService ProgrammingService WebcamService ProtectionService BEP

CommunicationService_SOURCES = CommunicationService.c $(IPCSockets) $(WebSockets) $(Utils)
CommunicationService_LDADD = -lwebsockets -lcjson -lsystemd -lpthread
CommunicationService_LDFLAGS = -g

ProgrammingService_SOURCES = ProgrammingService.c $(IPCSockets)
ProgrammingService_LDADD = -lwebsockets -lcjson -lsystemd -lpthread
ProgrammingService_LDFLAGS = -g

WebcamService_SOURCES = WebcamService.c $(IPCSockets) $(WebSockets)
WebcamService_LDADD = -lwebsockets -lcjson -lsystemd -lpthread
WebcamService_LDFLAGS = -g

ProtectionService_SOURCES = ProtectionService.c $(IPCSockets) $(SPI)
ProtectionService_LDADD = -lsystemd -lpthread -lbcm2835
ProtectionService_LDFLAGS = -g

BEP_SOURCES = parsers/ExpressionParsers.h parsers/BooleanExpressionParser.c $(Stack) $(Queue)
BEP_LDFLAGS = -g