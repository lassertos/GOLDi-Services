AUTOMAKE_OPTIONS = subdir-objects

GOLDiServicesdir = $(sysconfdir)/GOLDiServices

IPCSockets = interfaces/ipcsockets.h interfaces/ipcsockets.c
WebSockets = interfaces/websockets.h interfaces/websockets.c
Utils = utils/utils.h utils/utils.c
SPI = interfaces/spi.h interfaces/spi.c
Stack = utils/stack.h utils/stack.c
Queue = utils/queue.h utils/queue.c
JSON = parsers/json.h parsers/json.c
SensorsActuators = interfaces/SensorsActuators.h interfaces/SensorsActuators.c
BooleanExpressionParser = parsers/BooleanExpressionParser.h parsers/BooleanExpressionParser.c
StateMachine = parsers/StateMachine.h parsers/StateMachine.c
Programmers = programmer/goldi-programmer.h programmer/bcmGPIO.h programmer/avr-programmer.c programmer/bcmGPIO.c programmer/goldi-svf-player.c
Logging = logging/log.h logging/log.c

if CONTROLUNIT
bin_PROGRAMS = GOLDiCommunicationService GOLDiProgrammingService GOLDiCommandService
GOLDiCommunicationService_SOURCES = CommunicationServiceCU.c 
else
bin_PROGRAMS = GOLDiCommunicationService GOLDiProgrammingService GOLDiWebcamService GOLDiProtectionService GOLDiInitializationService
GOLDiCommunicationService_SOURCES = CommunicationServicePS.c 

GOLDiServicesExperimentsdir = $(GOLDiServicesdir)/experiments
GOLDiServices3AxisPortaldir = $(GOLDiServicesExperimentsdir)/3AxisPortal
GOLDiServices3AxisPortal_DATA = experiments/3AxisPortal/ExperimentData.json experiments/3AxisPortal/FPGA.svf
endif

GOLDiCommunicationService_SOURCES += $(IPCSockets) $(WebSockets) $(Utils) $(JSON) $(Logging)
GOLDiCommunicationService_LDADD = $(LWS_LIBS) -lcjson -lsystemd -lpthread
GOLDiCommunicationService_LDFLAGS = $(LWS_CFLAGS)
GOLDiCommunicationService_CPPFLAGS = -g -O0

GOLDiWebcamService_SOURCES = WebcamService.c $(IPCSockets) $(WebSockets) $(Utils) $(Logging) $(JSON)
GOLDiWebcamService_LDADD = $(LWS_LIBS) -lcjson -lsystemd -lpthread $(GSTREAMER_LIBS)
GOLDiWebcamService_LDFLAGS = $(LWS_CFLAGS) $(GSTREAMER_CFLAGS)
GOLDiWebcamService_CPPFLAGS = -g -O0

GOLDiProtectionService_SOURCES = ProtectionService.c $(IPCSockets) $(SPI) $(JSON) $(BooleanExpressionParser) $(Stack) $(Queue) $(Utils) $(SensorsActuators) $(Logging)
GOLDiProtectionService_LDADD = -lsystemd -lpthread -lbcm2835 -lcjson
GOLDiProtectionService_CPPFLAGS = -DEXTENDED_SENSORS_ACTUATORS -g -O0

GOLDiInitializationService_SOURCES = InitializationService.c $(JSON) $(Utils) $(StateMachine) $(SensorsActuators) $(BooleanExpressionParser) $(Stack) $(Queue) $(IPCSockets) $(Logging)
GOLDiInitializationService_LDADD = -lcjson -lpthread -lsystemd
GOLDiInitializationService_CPPFLAGS = -g -O0

GOLDiProgrammingService_SOURCES = ProgrammingService.c $(IPCSockets) $(Utils) $(Programmers) $(Logging)
GOLDiProgrammingService_LDADD = -lpthread -lsystemd -lbcm2835 -lxsvf
GOLDiProgrammingService_CPPFLAGS = -g -O0

GOLDiCommandService_SOURCES = CommandService.c $(IPCSockets) $(Utils) $(JSON) $(Logging) $(SensorsActuators) $(SPI)
GOLDiCommandService_LDADD = -lpthread -lsystemd -lbcm2835 -lcjson
GOLDiCommandService_CPPFLAGS = -g -O0