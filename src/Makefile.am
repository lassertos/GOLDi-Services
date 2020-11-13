AUTOMAKE_OPTIONS = subdir-objects

IPCSockets = interfaces/ipcsockets.h interfaces/ipcsockets.c
WebSockets = interfaces/websockets.h interfaces/websockets.c
Utils = utils/utils.h utils/utils.c
SPI = interfaces/spi.h interfaces/spi.c
Stack = utils/stack.h utils/stack.c
Queue = utils/queue.h utils/queue.c
JSON = parsers/json.h parsers/json.c
SensorsActuators = interfaces/SensorsActuators.h interfaces/SensorsActuators.c
ExpressionParsers = parsers/ExpressionParsers.h parsers/tinyexpr.h parsers/tinyexpr.c parsers/BooleanExpressionParser.c parsers/MathematicalExpression.c
StateMachine = parsers/StateMachine.h parsers/StateMachine.c
Programmers = programmer/goldi-programmer.h programmer/bcmGPIO.h programmer/avr-programmer.c programmer/bcmGPIO.c programmer/goldi-svf-player.c

bin_PROGRAMS = CommunicationServicePS ProgrammingService WebcamService ProtectionService InitializationService

CommunicationServicePS_SOURCES = CommunicationServicePS.c $(IPCSockets) $(WebSockets) $(Utils) $(JSON)
CommunicationServicePS_LDADD = -lwebsockets -lcjson -lsystemd -lpthread
CommunicationServicePS_LDFLAGS = -g

CommunicationServiceCU_SOURCES = CommunicationServiceCU.c $(IPCSockets) $(WebSockets) $(Utils) $(JSON)
CommunicationServiceCU_LDADD = -lwebsockets -lcjson -lsystemd -lpthread
CommunicationServiceCU_LDFLAGS = -g

WebcamService_SOURCES = WebcamService.c $(IPCSockets) $(WebSockets) $(Utils)
WebcamService_LDADD = -lwebsockets -lcjson -lsystemd -lpthread
WebcamService_LDFLAGS = -g

ProtectionService_SOURCES = ProtectionService.c $(IPCSockets) $(SPI) $(JSON) $(ExpressionParsers) $(Stack) $(Queue) $(Utils) $(SensorsActuators)
ProtectionService_LDADD = -lsystemd -lpthread -lbcm2835 -lcjson -lm
ProtectionService_LDFLAGS = -g
ProtectionService_CFLAGS = -DEXTENDED_SENSORS_ACTUATORS

InitializationService_SOURCES = InitializationService.c $(JSON) $(Utils) $(StateMachine) $(SensorsActuators) $(ExpressionParsers) $(Stack) $(Queue) $(IPCSockets)
InitializationService_LDADD = -lcjson -lm -lpthread -lsystemd
InitializationService_LDFLAGS = -g

ProgrammingService_SOURCES = ProgrammingService.c $(IPCSockets) $(Utils) $(Programmers)
ProgrammingService_LDADD = -lpthread -lsystemd -lbcm2835 -lxsvf
ProgrammingService_LDFLAGS = -g