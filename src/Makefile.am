AUTOMAKE_OPTIONS = subdir-objects

GOLDiServicesdir = $(sysconfdir)/GOLDiServices
GOLDiServicesLocaldir = /data/GOLDiServices

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
Logging = logging/log.h logging/log.c

if CONTROLUNIT
bin_PROGRAMS = CommunicationService ProgrammingService CommandService
CommunicationService_SOURCES = CommunicationServiceCU.c 

GOLDiServicesLocal_DATA = templates/DeviceDataCUTemplate.json
else
bin_PROGRAMS = CommunicationService ProgrammingService WebcamService ProtectionService InitializationService
CommunicationService_SOURCES = CommunicationServicePS.c 

GOLDiServicesExperimentsdir = $(GOLDiServicesdir)/experiments
GOLDiServices3AxisPortaldir = $(GOLDiServicesExperimentsdir)/3AxisPortal
GOLDiServices3AxisPortal_DATA = experiments/3AxisPortal/ExperimentData.json experiments/3AxisPortal/FPGA.svf
GOLDiServicesLocal_DATA = templates/DeviceDataPSTemplate.json
endif

CommunicationService_SOURCES += $(IPCSockets) $(WebSockets) $(Utils) $(JSON) $(Logging)
CommunicationService_LDADD = -lwebsockets -lcjson -lsystemd -lpthread
CommunicationService_LDFLAGS = -g

WebcamService_SOURCES = WebcamService.c $(IPCSockets) $(WebSockets) $(Utils) $(Logging) $(JSON)
WebcamService_LDADD = -lwebsockets -lcjson -lsystemd -lpthread
WebcamService_LDFLAGS = -g

ProtectionService_SOURCES = ProtectionService.c $(IPCSockets) $(SPI) $(JSON) $(ExpressionParsers) $(Stack) $(Queue) $(Utils) $(SensorsActuators) $(Logging)
ProtectionService_LDADD = -lsystemd -lpthread -lbcm2835 -lcjson -lm
ProtectionService_LDFLAGS = -g
ProtectionService_CPPFLAGS = -DEXTENDED_SENSORS_ACTUATORS

InitializationService_SOURCES = InitializationService.c $(JSON) $(Utils) $(StateMachine) $(SensorsActuators) $(ExpressionParsers) $(Stack) $(Queue) $(IPCSockets) $(Logging)
InitializationService_LDADD = -lcjson -lm -lpthread -lsystemd
InitializationService_LDFLAGS = -g

ProgrammingService_SOURCES = ProgrammingService.c $(IPCSockets) $(Utils) $(Programmers) $(Logging)
ProgrammingService_LDADD = -lpthread -lsystemd -lbcm2835 -lxsvf
ProgrammingService_LDFLAGS = -g

CommandService_SOURCES = CommandService.c $(IPCSockets) $(Utils) $(JSON) $(Logging) $(SensorsActuators) $(SPI)
CommandService_LDADD = -lpthread -lsystemd -lbcm2835 -lcjson
CommandService_LDFLAGS = -g