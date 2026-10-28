#pragma once

#include <QString>
#include <QStringList>
#include <QMap>
#include <QPointF>
#include <QUuid>
#include <QVector>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

namespace cad {

struct DrawingNoteAnnotation {
    QString id;
    QString text;
    double xMm = 12.0;
    double yMm = 180.0;
};

struct DrawingDimensionAnnotation {
    QString id;
    QString axis;
    double offsetMm = 9.0;
};

struct DrawingPartsListAnnotation {
    QString id;
    bool visible = false;
    double xMm = 12.0;
    double yMm = 12.0;
};

struct DrawingBalloonAnnotation {
    QString id;
    QString sourceBodyId;
    double xMm = 250.0;
    double yMm = 170.0;
};

struct DrawingDetailAnnotation {
    QString id;
    QString sourceView = QStringLiteral("FRONT");
    double sourceXmm = 0.0;
    double sourceYmm = 0.0;
    double captureRadiusMm = 8.0;
    double xMm = 240.0;
    double yMm = 85.0;
    double radiusMm = 20.0;
};

struct DrawingSectionAnnotation {
    QString id;
    QString planeAxis = QStringLiteral("Y");
    double depthFraction = 0.5;
    double xMm = 240.0;
    double yMm = 145.0;
};

struct DrawingSheetSettings {
    // Persistent sheet/view identities and editable projected-view centers (paper mm).
    QString sheetId;
    QString sourceGeometryToken;
    QStringList sourceBodyIds;
    QMap<QString,QString> viewIds;
    QMap<QString,QPointF> viewCenters;
    QVector<DrawingNoteAnnotation> noteAnnotations;
    QVector<DrawingDimensionAnnotation> dimensionAnnotations;
    DrawingPartsListAnnotation partsListAnnotation;
    QVector<DrawingBalloonAnnotation> balloonAnnotations;
    QVector<DrawingDetailAnnotation> detailAnnotations;
    QVector<DrawingSectionAnnotation> sectionAnnotations;
    QString projection = QStringLiteral("Third-angle");
    QString sheetSize = QStringLiteral("A4");
    QString auxiliaryView = QStringLiteral("Isometric");
    double sectionDepthFraction = 0.5;
    QStringList generalNotes;
    QString drawingNumber;
    QString drawnBy;
    QString checkedBy;
    QString issueDate;
    QString revision;
};

struct JointMotionKeyframe {
    double timeSeconds = 0.0;
    double value = 0.0; // degrees for revolute joints, millimetres for prismatic joints
};

struct Part {
    QString id;
    QString name;
    TopoDS_Shape shape;
    bool visible = true;
    QString color = QStringLiteral("#6c9dc2");
    // Changes only when the B-rep changes; lets transient subshape references
    // survive appearance edits and become invalid after topology mutations.
    QString geometryToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString materialName;
    double materialDensityKgPerM3 = 0.0;
    QString partNumber;
    QString description;
    QString revision;
    QString assemblyPath;
    QString procurementType = QStringLiteral("Manufactured");
    QString supplier;
    QString supplierPartNumber;
    // Persistent assembly identity. Bodies with the same definition ID are reusable component occurrences.
    QString componentDefinitionId;
    QString componentName;
    QString occurrenceId;
    double placementX = 0.0;
    double placementY = 0.0;
    double placementZ = 0.0;
    QString assemblyJointType;
    QString assemblyJointParentBodyId;
    double jointOriginX = 0.0, jointOriginY = 0.0, jointOriginZ = 0.0;
    double jointAxisX = 0.0, jointAxisY = 0.0, jointAxisZ = 1.0;
    double jointAngleDegrees = 0.0;
    double jointMinimumDegrees = -180.0, jointMaximumDegrees = 180.0;
    double jointTranslationMm = 0.0;
    double jointMinimumMm = 0.0, jointMaximumMm = 0.0;
    QVector<JointMotionKeyframe> jointMotionKeyframes;
    bool hasDrawingSettings = false;
    DrawingSheetSettings drawingSettings;
    // Focused editable history for a constrained rectangular XY sketch + extrusion.
    bool hasParametricDefinition = false;
    QString parametricGeometryToken;
    QMap<QString,double> namedParameters;
    QStringList sketchConstraints;
    QStringList featureHistory;
    QString holePatternAxis = QStringLiteral("X");
};

enum class DrawingSourceState { NoSavedSheet, Current, Stale, MissingBodies };
struct DrawingSheetSourceStatus {
    DrawingSourceState state = DrawingSourceState::NoSavedSheet;
    QStringList sourceBodyIds;
    QStringList missingSourceBodyIds;
    QString storedGeometryToken;
    QString currentGeometryToken;
};

struct BillOfMaterialsRow {
    int itemNumber = 0;
    QString partNumber;
    QString description;
    QString revision;
    QString materialName;
    int quantity = 0;
    double unitVolumeMm3 = 0.0;
    double totalVolumeMm3 = 0.0;
    double unitMassKg = 0.0;
    double totalMassKg = 0.0;
    QString assemblyPath;
    QString procurementType;
    QString supplier;
    QString supplierPartNumber;
    QString componentDefinitionId;
    QString componentName;
    QStringList bodyIds;
};

struct PartsListRow {
    int itemNumber = 0;
    QString bodyId;
    QString bodyName;
    QString partNumber;
    QString description;
    QString revision;
    QString materialName;
    int quantity = 1;
    double volumeMm3 = 0.0;
    double massKg = 0.0;
    QString assemblyPath;
    QString procurementType;
    QString supplier;
    QString supplierPartNumber;
    QString componentDefinitionId;
    QString componentName;
    QString occurrenceId;
    double placementX = 0.0;
    double placementY = 0.0;
    double placementZ = 0.0;
};

enum class StandardsProfile { BomData, IsoDrawingReadiness, AsmeDrawingReadiness };
enum class FindingSeverity { Info, Warning, Error };
struct StandardsFinding {
    FindingSeverity severity = FindingSeverity::Info;
    QString code;
    QString bodyId;
    QString message;
};
struct StandardsCheckReport {
    StandardsProfile profile = StandardsProfile::BomData;
    QVector<StandardsFinding> findings;
    int errors = 0;
    int warnings = 0;
    bool passed() const noexcept { return errors == 0; }
};

// Project-level drawing content checklist defaults. These drive structural
// review only and do not represent ISO/ASME certification.
struct DrawingReadinessPolicy {
    QStringList requiredTitleBlockFields;
    QStringList requiredAnnotationConventions;
};

struct MinimumDistanceResult {
    double distance = 0.0;
    double firstX = 0.0;
    double firstY = 0.0;
    double firstZ = 0.0;
    double secondX = 0.0;
    double secondY = 0.0;
    double secondZ = 0.0;
};

struct InterferenceResult {
    double volume = 0.0;
    double centroidX = 0.0;
    double centroidY = 0.0;
    double centroidZ = 0.0;
};

struct BodyDiagnostics {
    bool valid = false;
    int solids = 0;
    int shells = 0;
    int faces = 0;
    int wires = 0;
    int edges = 0;
    int vertices = 0;
    double volume = 0.0;
    double surfaceArea = 0.0;
    double minX = 0.0, minY = 0.0, minZ = 0.0;
    double maxX = 0.0, maxY = 0.0, maxZ = 0.0;
    double centerX = 0.0, centerY = 0.0, centerZ = 0.0;
};

struct DensityMassProperties {
    double volume = 0.0; // mm^3
    double densityKgPerM3 = 0.0;
    double massKg = 0.0;
    double centerX = 0.0, centerY = 0.0, centerZ = 0.0;
};

struct SectionMeasurement {
    int edgeCount = 0;
    double totalLength = 0.0; // mm
};

struct SurfaceTypeSummary {
    int planarFaces = 0;
    int cylindricalFaces = 0;
    int conicalFaces = 0;
    int sphericalFaces = 0;
    int toroidalFaces = 0;
    int otherFaces = 0;
    double planarArea = 0.0; // mm^2
    double cylindricalArea = 0.0;
    double conicalArea = 0.0;
    double sphericalArea = 0.0;
    double toroidalArea = 0.0;
    double otherArea = 0.0;
};

struct FaceAngleMeasurement {
    double angleDegrees = 0.0;
    bool parallel = false;
    double planeSeparationMm = 0.0;
    double firstNormalX = 0.0;
    double firstNormalY = 0.0;
    double firstNormalZ = 0.0;
    double secondNormalX = 0.0;
    double secondNormalY = 0.0;
    double secondNormalZ = 0.0;
};

struct CylindricalFaceMeasurement {
    double radiusMm = 0.0;
    double diameterMm = 0.0;
    double axisX = 0.0;
    double axisY = 0.0;
    double axisZ = 0.0;
    double originX = 0.0;
    double originY = 0.0;
    double originZ = 0.0;
};

struct FaceReference {
    QString partId;
    QString geometryToken;
    int faceIndex = 0; // 1-based TopExp face traversal index for this geometryToken.
};

struct EdgeReference {
    QString partId;
    QString geometryToken;
    int edgeIndex = 0; // 1-based TopExp edge traversal index within this geometryToken.
};

struct VertexReference {
    QString partId;
    QString geometryToken;
    int vertexIndex = 0;
};

struct FilletCommand {
    EdgeReference edge;
    QVector<EdgeReference> edges;
    TopoDS_Shape sourceShape;
    TopoDS_Edge sourceEdge;
    QVector<TopoDS_Edge> sourceEdges;
    double radius = 0.0;
};

struct FilletPreview {
    TopoDS_Shape shape;
    QString error;
};

struct ChamferCommand {
    EdgeReference edge;
    QVector<EdgeReference> edges;
    TopoDS_Shape sourceShape;
    TopoDS_Edge sourceEdge;
    QVector<TopoDS_Edge> sourceEdges;
    TopoDS_Face referenceFace;
    double distance = 0.0;
    double secondDistance = 0.0;
};

struct ChamferPreview {
    TopoDS_Shape shape;
    QString error;
};

enum class SketchProfile { Rectangle, Circle, Ellipse, Slot, Triangle, RegularPolygon, RoundedRectangle, Ring };
enum class SketchPlane { XY, XZ, YZ, Custom };
enum class ExtrusionMode { NewBody, Join, Cut };

struct ExtrusionCommand {
    SketchProfile profile = SketchProfile::Rectangle;
    SketchPlane sketchPlane = SketchPlane::XY;
    bool reverseDirection = false;
    bool throughAll = false;
    ExtrusionMode mode = ExtrusionMode::NewBody;
    QString targetPartId;
    QString targetGeometryToken;
    TopoDS_Shape targetShape;
    double originX = 0.0;
    double originY = 0.0;
    double planeOffset = 0.0;
    // Custom sketch frame in world coordinates. The U direction is projected
    // into the plane; V is derived as normal × U to form a right-handed frame.
    double customOriginX = 0.0;
    double customOriginY = 0.0;
    double customOriginZ = 0.0;
    double customNormalX = 0.0;
    double customNormalY = 0.0;
    double customNormalZ = 1.0;
    double customUAxisX = 1.0;
    double customUAxisY = 0.0;
    double customUAxisZ = 0.0;
    double widthOrDiameter = 0.0;
    double depth = 0.0;
    double height = 0.0;
    bool symmetric = false;
    double taperDegrees = 0.0;
    double cornerRadius = 0.0;
    quint64 sourceRevision = 0;
};

struct ExtrusionPreview {
    TopoDS_Shape shape;
    QString error;
};

struct RevolveCommand {
    double radialOffset = 0.0;
    double axialOrigin = 0.0;
    double radialWidth = 0.0;
    double axialHeight = 0.0;
    double angleDegrees = 360.0;
    int axisIndex = 2; // 0=X, 1=Y, 2=Z
    double axisOriginX = 0.0;
    double axisOriginY = 0.0;
    double axisOriginZ = 0.0;
    double axisDirectionX = 0.0;
    double axisDirectionY = 0.0;
    double axisDirectionZ = 1.0;
    quint64 sourceRevision = 0;
};

struct RevolvePreview {
    TopoDS_Shape shape;
    QString error;
};

enum class HoleStyle { Simple, Counterbore, Countersink };

struct HoleCommand {
    QString partId;
    QString geometryToken;
    TopoDS_Shape sourceShape;
    double x = 0.0;
    double y = 0.0;
    double entryZ = 0.0;
    double diameter = 0.0;
    double depth = 0.0;
    bool throughAll = false;
    HoleStyle style = HoleStyle::Simple;
    double headDiameter = 0.0;
    double headDepth = 0.0;
    double sinkAngleDegrees = 90.0;
};

struct HolePreview {
    TopoDS_Shape shape;
    QString error;
};

struct DovetailGrooveCommand {
    QString partId;
    QString geometryToken;
    TopoDS_Shape sourceShape;
    double centerX = 0.0;
    double centerY = 0.0;
    double entryZ = 0.0;
    double mouthWidth = 0.0;
    double bottomWidth = 0.0;
    double depth = 0.0;
    double length = 0.0;
    double angleDegrees = 0.0;
};

struct DovetailGroovePreview {
    TopoDS_Shape shape;
    QString error;
};

struct TSlotGrooveCommand {
    QString partId;
    QString geometryToken;
    TopoDS_Shape sourceShape;
    double centerX = 0.0;
    double centerY = 0.0;
    double entryZ = 0.0;
    double neckWidth = 0.0;
    double undercutWidth = 0.0;
    double depth = 0.0;
    double undercutHeight = 0.0;
    double length = 0.0;
    double angleDegrees = 0.0;
};

struct TSlotGroovePreview {
    TopoDS_Shape shape;
    QString error;
};

struct RoundedSlotCommand {
    QString partId;
    QString geometryToken;
    TopoDS_Shape sourceShape;
    double centerX = 0.0;
    double centerY = 0.0;
    double entryZ = 0.0;
    double width = 0.0;
    double length = 0.0;
    double depth = 0.0;
    double angleDegrees = 0.0;
};

struct RoundedSlotPreview {
    TopoDS_Shape shape;
    QString error;
};

struct KeyholeSlotCommand {
    QString partId;
    QString geometryToken;
    TopoDS_Shape sourceShape;
    double centerX = 0.0;
    double centerY = 0.0;
    double entryZ = 0.0;
    double headDiameter = 0.0;
    double throatWidth = 0.0;
    double throatLength = 0.0;
    double depth = 0.0;
    double angleDegrees = 0.0;
};

struct KeyholeSlotPreview {
    TopoDS_Shape shape;
    QString error;
};

struct RoundedSlotPatternCommand {
    QString partId;
    QString geometryToken;
    TopoDS_Shape sourceShape;
    double centerX = 0.0;
    double centerY = 0.0;
    double entryZ = 0.0;
    double width = 0.0;
    double length = 0.0;
    double depth = 0.0;
    double angleDegrees = 0.0;
    int instanceCount = 0;
    double spacingX = 0.0;
    double spacingY = 0.0;
};

struct RoundedSlotPatternPreview {
    TopoDS_Shape shape;
    QString error;
};

enum class Primitive { Box, Cylinder, Sphere, Cone, Torus, RegularPrism, Capsule, Ellipsoid, Tube, Wedge, PyramidFrustum, HexNut };
enum class EngineeringPart : int {
    SteppedShaft, KeyedShaft, SplinedShaft, SolidPulley, VGroovePulley, MultiGroovePulley,
    FlangedBearing, BallBearing, FlangedPipe, BoltCircleFlange, SquareFlange, BlindFlange,
    PipeElbow, PipeReducer, PipeTee, PipeYBranch, PipeCap, GroovedPipePlug,
    AngleBracket, GussetBracket, TeeBracket, UChannel, CChannel, IBeam, TBeam, DovetailRail,
    SpurGear, RackGear, Sprocket, FanImpeller, Count
};
enum class AdvancedPart : int {
    HexBolt, SocketHeadScrew, FlatWasher, ToothLockWasher, DowelPin, ClevisPin,
    TaperPin, ShaftCollar, FlangeCoupling, JawCoupling, PillowBlock, SplitBearingHousing,
    LinearGuideRail, LinearBearingBlock, BallScrewSupport, CoilSpring, TimingPulley,
    TimingBelt, BrakeRotor, VentedBrakeRotor, ClevisBracket, HingeBarrel, PipeUnion,
    ValveBody, CheckValve, ReducingNozzle, HeatSink, ConveyorRoller, PumpImpeller,
    UniversalJoint, Count
};
enum class SpecializedPart : int {
    HelicalGear, BevelGear, WormWheel, WormShaft, PlanetCarrier, PlanetaryGearSet,
    IdlerPulley, TimingBeltTensioner, NeedleBearing, TaperedRollerBearing, ThrustBearing,
    SphericalRollerBearing, HydraulicCylinder, PneumaticCylinder, LinearActuator, TieRodCylinder,
    Crankshaft, ConnectingRod, Piston, PistonRing, CylinderLiner, CamFollower, EccentricCam,
    ChainLink, RollerChainSegment, DoubleRowSprocket, FlexibleBellowsCoupling, DiscCoupling,
    OldhamCoupling, YStrainer, HoseBarb, PipeElbow, PipeCross, ExpansionJoint, CoolingFan, Count
};
enum class IndustrialPart : int {
    DifferentialHousing, ConstantVelocityJoint, TripodJoint, TorqueLimiter, CentrifugalClutch,
    MultiDiscClutchPack, Flywheel, Flexplate, BrakeCaliper, BrakeDrum, ClutchPressurePlate,
    CrankshaftPulley, HarmonicBalancer, GearboxHousing,
    AngularContactBearing, DoubleRowBallBearing, ThinSectionBearing, CylindricalRollerBearing,
    SphericalPlainBearing, BronzeSleeveBushing, FlangedSleeveBushing, BearingLocknut,
    BearingRetainerPlate, BearingSpacer, LabyrinthSeal, RadialShaftSeal, OilSealHousing,
    BallValveBody, GateValveBody, ButterflyValveBody, GlobeValveBody, PressureReliefValve,
    SolenoidValveBody, FlowMeterHousing, HydraulicManifoldBlock, WeldNeckFlange, LapJointFlange,
    PipeSaddleSupport, CompressionTubeElbow, BulkheadFitting, TankDrainFitting, HoseReducer,
    FireHoseNozzle,
    CentrifugalPumpCasing, VolutePumpCover, PlateHeatExchanger, ShellTubeExchanger, RadiatorCore,
    ExpansionTank, FluidReservoir, PressureVesselShell, EllipticalVesselHead, SightGlassAssembly,
    FilterCanister, OilFilterHousing, IntakeManifold, ExhaustManifold,
    TSlotExtrusion, MachineToolingPlate, ElectronicsEnclosure, MotorFrame, MotorEndBell, StatorCore,
    RotorCore, FanShroud, CableGland, StraightCableTray, PerforatedCableTray, PipeShoeSupport,
    PipeHanger, Count
};
enum class ExtendedPart : int {
    ConveyorDriveDrum, ConveyorBendPulley, BeltTakeUpScrew, BeltTrackingRoller,
    BucketElevatorHeadPulley, ScrewConveyorEndPlate, RollerConveyorSideFrame,
    PalletRackFootPlate, HoistCableDrum, CraneTrolleyWheel,
    LatheChuckBackplate, RotaryTableFaceplate, ViseFixedJaw, ViseSwivelBase,
    SpindleBearingCartridge, TailstockQuill, ColletChuck, ToolSetterBlock, PressRam,
    MachineLevelingFoot,
    ScrollCompressorShell, HermeticCompressorCover, CondenserHeader,
    EvaporatorDistributor, RefrigerantFilterDrier, ExpansionValveBody,
    BlowerScrollHousing, DuctTransition, DuctAccessPanel, HeatPumpAccumulator,
    TractorAxleHub, ImplementHitchPlate, DrawbarClevis, PlowShare, CultivatorTineClamp,
    SeedMeteringHousing, IrrigationSprinklerHead, DripLineManifold, HarvesterPulley,
    MowerBladeCarrier,
    PropellerShaftSternTube, RudderPintle, SteeringQuadrant, SeacockBody,
    ThroughHullFitting, DeckCleatBase, AnchorChainStopper, BilgePumpStrainer,
    TankInspectionHatch, MastStepPlate,
    BusbarSupportInsulator, CableLadderRung, CableTraySplicePlate, JunctionBoxBracket,
    DinRailEndStop, TransformerMountingFoot, GroundingLug, EnclosureDoorHinge,
    ControlPanelBackplate, PowerElectronicsHeatSink,
    CycloneSeparatorBody, DustCollectorHopper, BagFilterTubeSheet,
    RotaryAirlockHousing, SlurryPumpCasing, GearPumpCover, HydraulicAccumulatorShell,
    PneumaticManifoldBlock, QuickCouplerBody, SamplingPortHousing, Count
};
enum class PrecisionPart : int {
    Servomountplate, Robotwristflange, Endeffectorjaw, Linearslidecarriage, Rotaryindextable, Cablecarrierlink, Sensorbracket, Actuatorclevis, Encoderhousing, Suctioncupmanifold, Microscopestage, Centrifugerotor, Pipettecarousel, Labbottlecap, Syringepumpcarriage, Instrumentpanelplate, Samplevialrack, Medicalgasoutlet, Oxygenregulatorbody, Sterilizerdoorflange, Lenscell, Mirrormount, Telescopefocusertube, Opticalbenchpost, Cameracageplate, Filterwheelhub, Prismholder, Laserdiodeheatsink, Fiberspoolhub, Lidarsensorhousing, Windturbinehub, Solartrackerbearing, Batterymoduleendplate, Inverterbusbarblock, Fuelcellmanifold, Electrolyzerendcap, Geothermalwellheadflange, Hydrantnozzle, Watermeterbody, Pressuregaugecase, Scaffoldingbaseplate, Guardrailpostshoe, Anchorbolttemplate, Bridgebearingplate, Expansionjointanchor, Handrailelbow, Piperacksaddle, Cranehookblock, Liftinglugplate, Structuralbaseshoe, Traileraxlespindle, Wheelspacer, Brakemastercylinderbody, Steeringrackend, Shockabsorberclevis, Seatrailbracket, Wipermotorhousing, Roofrackfoot, Bicyclecrankspider, Scooterforkcrown, Laptophingebarrel, Cameratripodsocket, Speakerbasket, Headphoneyoke, Gamecontrollershell, Powertoolgearboxcover, Batteryterminalblock, Appliancemotormount, Smartthermostatplate, Dronemotormount, Count
};
enum class ProductionPart : int {
    Wingrib, Wingsparrootfitting, Flaphingebracket, Landinggeartrunnion, Enginemountring, Turbinebladeroot, Aircraftseattrackfitting, Pitottubehousing, Fuellinebulkheadfitting, Avionicsrackrail, Railfasteningclip, Railjointfishplate, Trackgaugespacer, Bogieaxleboxcover, Brakedisccarrier, Couplerknucklepin, Pantographinsulatormount, Signallamphousing, Overheadwiresteadyarm, Tramdoorroller, Excavatorbuckettoothadapter, Trackrollercarrier, Idlerwheelhub, Hydraulicbreakerretainer, Conveyorimpactbedbracket, Drillrodcoupling, Mineventilationductflange, Rockcrusherjawplate, Screeningdeckclamp, Loaderliftcylindereye, Bottlecappingchuck, Fillernozzlebody, Conveyorguiderail, Cartonerectorcamfollower, Labelerrollerhub, Rotarytablestarwheel, Mixerpaddlehub, Sanitarypipeclamp, Tanksprayball, Foodpumprotor, Routercolletnut, Sawarborflange, Bandsawbladeguide, Planercutterhead, Dusthoodport, Clampscrewhandle, Weldfixturelocator, Weldingtorchholder, Sheetmetalbrakefinger, Plasmatorchstandoff, Windtowerflange, Nacelleyawbearingring, Solarpanelmidclamp, Cablecleat, Insulatorcap, Switchgearbusductelbow, Transformerbushingturret, Batteryrackcrossbar, EVchargingconnectorshell, Substationgroundingbar, Bicyclepedalspindle, Bicycleheadsetcup, Wheelhubshell, Skibindingplate, Skateboardtruckhanger, Treadmillrollershaft, Rowingmachinepulley, Weightstackguideblock, Exercisecableswivel, Climbinganchorplate, Hydrantvalvebonnet, Firesprinklerescutcheon, Waterpumpcoupling, Metermountingbracket, Sewercleanoutcap, Piperepairsaddle, Checkvalvedisc, Backflowpreventerbody, Irrigationfilterbowl, Draingrateframe, Spinningspindlesleeve, Loomshuttleguide, Textilerollcore, Yarntensionpulley, Fabriccuttercarriage, Printerplatenroller, Paperfeedgear, Inkcartridgecarriage, Labelapplicatorbracket, Bindingpressplaten, PLCmountingrail, Robotcablegland, Safetylightcurtainbracket, Linearencoderscalemount, Pneumaticgripperfinger, Vacuumejectorbody, Machineguardhinge, Limitswitchbracket, Conveyorphotoeyemount, Palletizerforkplate, Count
};
enum class ModelingTool : int {
    RectangularTube, SquareTube, ZSection, HatChannel, TSlotRail,
    SlottedPlate, GridHolePlate, BoltCircleFlange, SquareFlange, LiftingLug,
    ClevisBracket, HingeBarrel, RibbedPlate, TriangularGusset, EllipticalTube,
    TaperedTube, PipeElbow, PipeTee, PipeYBranch, ConcentricReducer,
    EccentricReducer, BlindFlange, FlangedPipe, VGroovePulley, TimingPulley,
    RackGear, ChainSprocket, FanImpeller, BallBearingAssembly, CoilSpring, Count
};
struct EngineeringPartSpec {
    QString name;
    QStringList labels;
    QVector<double> defaults;
    int geometryFamily = 0;
};
EngineeringPartSpec engineeringPartSpec(EngineeringPart kind);
EngineeringPartSpec advancedPartSpec(AdvancedPart kind);
EngineeringPartSpec specializedPartSpec(SpecializedPart kind);
EngineeringPartSpec industrialPartSpec(IndustrialPart kind);
EngineeringPartSpec extendedPartSpec(ExtendedPart kind);
EngineeringPartSpec precisionPartSpec(PrecisionPart kind);
EngineeringPartSpec productionPartSpec(ProductionPart kind);
EngineeringPartSpec modelingToolSpec(ModelingTool kind);
enum class BooleanOp { Fuse, Cut, Common };
enum class Axis { X, Y, Z };
struct ShellOpeningFace { Axis axis = Axis::Z; bool positiveSide = true; };

// Deliberately a direct-modeling document. A feature dependency graph is NOT present in v1.0.
// Every geometry mutation is committed only after OCCT succeeds and validates the result.
class SceneDocument {
public:
    const QVector<Part>& parts() const noexcept { return parts_; }
    bool dirty() const noexcept { return revision_ != savedRevision_; }
    QString path() const { return path_; }
    const Part* find(const QString& id) const;
    const DrawingReadinessPolicy& drawingReadinessPolicy() const noexcept { return drawingReadinessPolicy_; }
    bool setDrawingReadinessPolicy(const DrawingReadinessPolicy& policy,QString* error = nullptr);

    bool addPrimitive(Primitive kind, const QVector<double>& dimensions, QString* error = nullptr);
    bool addEngineeringPart(EngineeringPart kind, const QVector<double>& dimensions, QString* error = nullptr);
    bool addAdvancedPart(AdvancedPart kind, const QVector<double>& dimensions, QString* error = nullptr);
    bool addSpecializedPart(SpecializedPart kind, const QVector<double>& dimensions, QString* error = nullptr);
    bool addIndustrialPart(IndustrialPart kind, const QVector<double>& dimensions, QString* error = nullptr);
    bool addExtendedPart(ExtendedPart kind, const QVector<double>& dimensions, QString* error = nullptr);
    bool addPrecisionPart(PrecisionPart kind, const QVector<double>& dimensions, QString* error = nullptr);
    bool addProductionPart(ProductionPart kind, const QVector<double>& dimensions, QString* error = nullptr);
    bool addModelingTool(ModelingTool kind, const QVector<double>& dimensions, QString* error = nullptr);
    bool addOrientedCylinder(double radius,double height,double ox,double oy,double oz,double ax,double ay,double az,
                             QString* error = nullptr);
    bool addOrientedCone(double bottomRadius,double topRadius,double height,double ox,double oy,double oz,
                         double ax,double ay,double az,QString* error = nullptr);
    bool addTorusSegment(double majorRadius,double tubeRadius,double angleDegrees,QString* error = nullptr);
    bool sweepCircularProfile(double radius,double pathLength,double lateralOffset,double verticalRise,
                              QString* error = nullptr);
    bool sweepRectangularProfile(double width,double height,double pathLength,double lateralOffset,double verticalRise,
                                 QString* error = nullptr);
    bool sweepEllipticalProfile(double semiAxisA,double semiAxisB,double pathLength,double lateralOffset,double verticalRise,
                                QString* error = nullptr);
    bool addHelicalSweep(double profileRadius,double coilRadius,double pitch,double turns,QString* error = nullptr);
    bool addVariablePitchHelicalSweep(double profileRadius,double coilRadius,double startPitch,double endPitch,
                                      double turns,QString* error = nullptr);
    bool addTaperedHelicalSweep(double profileRadius,double startCoilRadius,double endCoilRadius,double pitch,
                                double turns,QString* error = nullptr);
    bool addEllipticalPrism(double semiAxisA,double semiAxisB,double height,QString* error = nullptr);
    bool addEllipticalTube(double outerA,double outerB,double innerA,double innerB,double height,
                           QString* error = nullptr);
    bool loftCircularSections(double bottomRadius,double topRadius,double height,double offsetX,double offsetY,
                              QString* error = nullptr);
    bool loftRectangleToCircle(double width,double depth,double radius,double height,double offsetX,double offsetY,
                               QString* error = nullptr);
    bool loftHollowReducer(double bottomOuter,double bottomInner,double topOuter,double topInner,double height,
                           double offsetX,double offsetY,QString* error = nullptr);
    bool loftRectangles(double bottomX,double bottomY,double bottomWidth,double bottomDepth,
                        double topX,double topY,double topWidth,double topDepth,double height,
                        QString* error = nullptr);
    bool loftEllipticalSections(double bottomSemiAxisA,double bottomSemiAxisB,
                                double topSemiAxisA,double topSemiAxisB,double height,
                                double offsetX,double offsetY,QString* error = nullptr);
    bool translate(const QString& id, double x, double y, double z, QString* error = nullptr);
    bool rotate(const QString& id, Axis axis, double degrees, QString* error = nullptr);
    bool scale(const QString& id, double factor, QString* error = nullptr);
    bool scaleNonUniform(const QString& id,double factorX,double factorY,double factorZ,QString* error = nullptr);
    bool duplicate(const QString& id, double dx, double dy, double dz, QString* error = nullptr);
    bool makeComponent(const QString& id,const QString& componentName,QString* error = nullptr);
    bool insertComponentOccurrence(const QString& sourceBodyId,double x,double y,double z,QString* error = nullptr);
    bool synchronizeComponentOccurrences(const QString& sourceBodyId,QString* error = nullptr);
    bool addRigidJoint(const QString& parentBodyId,const QString& childBodyId,QString* error = nullptr);
    bool addRevoluteJoint(const QString& parentBodyId,const QString& childBodyId,
                          double originX,double originY,double originZ,double axisX,double axisY,double axisZ,
                          double minimumDegrees,double maximumDegrees,QString* error = nullptr);
    bool driveRevoluteJoint(const QString& childBodyId,double angleDegrees,QString* error = nullptr);
    bool addPrismaticJoint(const QString& parentBodyId,const QString& childBodyId,
                           double axisX,double axisY,double axisZ,double minimumMm,double maximumMm,
                           QString* error = nullptr);
    bool drivePrismaticJoint(const QString& childBodyId,double offsetMm,QString* error = nullptr);
    // Applies a set of revolute-angle (degrees) and prismatic-offset (mm) targets
    // parent-first as one undoable assembly-pose solve.
    bool solveAssemblyPose(const QMap<QString,double>& jointPositions,QString* error = nullptr);
    bool setJointMotionKeyframes(const QString& childBodyId,const QVector<JointMotionKeyframe>& keyframes,
                                 QString* error = nullptr);
    bool evaluateAssemblyMotionStudy(double timeSeconds,QString* error = nullptr);
    bool setDrawingSheetSettings(const QString& partId,const DrawingSheetSettings& settings,QString* error = nullptr);
    DrawingSheetSourceStatus drawingSheetSourceStatus(const QString& partId) const;
    bool refreshDrawingSheetSource(const QString& partId,QString* error = nullptr);
    bool addParametricRectangularExtrusion(double width,double height,double depth,QString* error = nullptr);
    bool addParametricRectangularExtrusionAt(double originX,double originY,double width,double height,double depth,QString* error = nullptr);
    bool addParametricThroughHole(const QString& partId,double diameter,double centerX,double centerY,QString* error = nullptr);
    bool addParametricLinearHolePattern(const QString& partId,int holeCount,double spacingMm,QString* error = nullptr);
    bool addParametricLinearHolePattern(const QString& partId,int holeCount,double spacingMm,
                                        const QString& axis,QString* error = nullptr);
    bool setNamedParameter(const QString& partId,const QString& parameterName,double value,QString* error = nullptr);
    bool setNamedParameters(const QString& partId,const QMap<QString,double>& values,QString* error = nullptr);
    QMap<QString,double> namedParameters(const QString& partId) const;
    QStringList sketchConstraints(const QString& partId) const;
    QStringList featureHistory(const QString& partId) const;
    bool parametricDefinitionCurrent(const QString& partId) const;
    bool booleanOperation(BooleanOp op, const QString& firstId, const QString& secondId, QString* error = nullptr);
    bool measureMinimumDistance(const QString& firstId,const QString& secondId,MinimumDistanceResult* result,
                                QString* error = nullptr) const;
    bool measureInterference(const QString& firstId,const QString& secondId,InterferenceResult* result,
                             QString* error = nullptr) const;
    bool diagnoseBody(const QString& id,BodyDiagnostics* result,QString* error = nullptr) const;
    bool measureMassFromDensity(const QString& id,double densityKgPerM3,
                                DensityMassProperties* result,QString* error = nullptr) const;
    bool measurePlanarSection(const QString& id,int normalAxis,double offset,
                              SectionMeasurement* result,QString* error = nullptr) const;
    bool summarizeSurfaceTypes(const QString& id,SurfaceTypeSummary* result,
                               QString* error = nullptr) const;
    bool measureFaceArea(const FaceReference& face,double* area,QString* error = nullptr) const;
    bool measureAngleBetweenPlanarFaces(const FaceReference& first,const FaceReference& second,
                                        FaceAngleMeasurement* result,QString* error = nullptr) const;
    bool measureCylindricalFace(const FaceReference& face,CylindricalFaceMeasurement* result,
                                QString* error = nullptr) const;
    bool measureEdgeLength(const EdgeReference& edge,double* length,QString* error = nullptr) const;
    bool measureVertexCoordinates(const VertexReference& vertex,double* x,double* y,double* z,QString* error = nullptr) const;
    bool measureVertexDistance(const VertexReference& first,const VertexReference& second,double* distance,QString* error = nullptr) const;
    bool filletEdge(const EdgeReference& edge, double radius, QString* error = nullptr);
    bool filletAllEdges(const QString& id,double radius,QString* error = nullptr);
    bool prepareFillet(const EdgeReference& edge, double radius, FilletCommand* command, QString* error = nullptr) const;
    bool prepareFillet(const QVector<EdgeReference>& edges,double radius,FilletCommand* command,QString* error = nullptr) const;
    static FilletPreview buildFilletPreview(const FilletCommand& command);
    bool commitFillet(const FilletCommand& command, const TopoDS_Shape& preview, QString* error = nullptr);
    bool chamferEdge(const EdgeReference& edge, double distance, QString* error = nullptr);
    bool chamferAllEdges(const QString& id,double distance,QString* error = nullptr);
    bool draftFace(const QString& id,int faceIndex,Axis pullAxis,double neutralPlaneOffset,double angleDegrees,
                   QString* error = nullptr);
    bool prepareChamfer(const EdgeReference& edge, double distance, ChamferCommand* command, QString* error = nullptr) const;
    bool prepareChamfer(const QVector<EdgeReference>& edges,double distance,ChamferCommand* command,QString* error = nullptr) const;
    bool prepareAsymmetricChamfer(const EdgeReference& edge,double distanceOnReferenceFace,
                                  double distanceOnOppositeFace,ChamferCommand* command,
                                  QString* error = nullptr) const;
    static ChamferPreview buildChamferPreview(const ChamferCommand& command);
    bool commitChamfer(const ChamferCommand& command, const TopoDS_Shape& preview, QString* error = nullptr);
    bool prepareExtrusion(SketchProfile profile, double originX, double originY,
                          double widthOrDiameter, double depth, double height,
                          ExtrusionCommand* command, QString* error = nullptr) const;
    bool prepareExtrusion(SketchProfile profile, double originX, double originY,
                          double widthOrDiameter, double depth, double height,
                          ExtrusionMode mode, const QString& targetPartId,
                          ExtrusionCommand* command, QString* error = nullptr) const;
    bool prepareSymmetricExtrusion(SketchProfile profile,double originX,double originY,double widthOrDiameter,
                                   double depth,double height,ExtrusionMode mode,const QString& targetPartId,
                                   ExtrusionCommand* command,QString* error = nullptr) const;
    bool prepareTaperedExtrusion(SketchProfile profile,double originX,double originY,double widthOrDiameter,
                                 double depth,double height,double taperDegrees,ExtrusionMode mode,
                                 const QString& targetPartId,ExtrusionCommand* command,QString* error = nullptr) const;
    bool prepareRoundedRectangleExtrusion(double originX,double originY,double width,double depth,double cornerRadius,
                                          double height,ExtrusionMode mode,const QString& targetPartId,
                                          ExtrusionCommand* command,QString* error = nullptr) const;
    static ExtrusionPreview buildExtrusionPreview(const ExtrusionCommand& command);
    bool commitExtrusion(const ExtrusionCommand& command, const TopoDS_Shape& preview, QString* error = nullptr);
    bool prepareRevolve(double radialOffset, double axialOrigin, double radialWidth,
                        double axialHeight, double angleDegrees, RevolveCommand* command,
                        QString* error = nullptr) const;
    bool prepareRevolveAboutAxis(double radialOffset,double axialOrigin,double radialWidth,double axialHeight,
                                 double angleDegrees,int axisIndex,double originX,double originY,double originZ,
                                 RevolveCommand* command,QString* error = nullptr) const;
    bool prepareRevolveAboutCustomAxis(double radialOffset,double axialOrigin,double radialWidth,double axialHeight,
                                      double angleDegrees,double originX,double originY,double originZ,
                                      double directionX,double directionY,double directionZ,
                                      RevolveCommand* command,QString* error = nullptr) const;
    static RevolvePreview buildRevolvePreview(const RevolveCommand& command);
    bool commitRevolve(const RevolveCommand& command, const TopoDS_Shape& preview, QString* error = nullptr);
    bool mirror(const QString& id, Axis planeNormal, double planeOffset, QString* error = nullptr);
    bool mirrorAcrossPlane(const QString& id, double nx, double ny, double nz, double planeOffset,
                           QString* error = nullptr);
    bool rotateAboutAxis(const QString& id, double ox, double oy, double oz,
                         double ax, double ay, double az, double degrees, QString* error = nullptr);
    bool linearPattern(const QString& id, Axis axis, int instanceCount, double spacing, QString* error = nullptr);
    bool linearPatternAlongVector(const QString& id,double directionX,double directionY,double directionZ,
                                  int instanceCount,double spacing,QString* error = nullptr);
    bool rectangularPattern(const QString& id, Axis firstAxis, int firstCount, double firstSpacing,
                            Axis secondAxis, int secondCount, double secondSpacing, QString* error = nullptr);
    bool rectangularPatternAlongVectors(const QString& id,double firstX,double firstY,double firstZ,
                                        int firstCount,double firstSpacing,double secondX,double secondY,
                                        double secondZ,int secondCount,double secondSpacing,QString* error = nullptr);
    bool prepareHole(const QString& id, double x, double y, double entryZ, double diameter,
                     double depth, bool throughAll, HoleCommand* command, QString* error = nullptr) const;
    bool prepareCounterbore(const QString& id,double x,double y,double entryZ,double diameter,double depth,
                            bool throughAll,double headDiameter,double headDepth,HoleCommand* command,
                            QString* error = nullptr) const;
    bool prepareCountersink(const QString& id,double x,double y,double entryZ,double diameter,double depth,
                            bool throughAll,double headDiameter,double includedAngleDegrees,HoleCommand* command,
                            QString* error = nullptr) const;
    static HolePreview buildHolePreview(const HoleCommand& command);
    bool commitHole(const HoleCommand& command, const TopoDS_Shape& preview, QString* error = nullptr);
    bool prepareDovetailGroove(const QString& id,double centerX,double centerY,double entryZ,
                               double mouthWidth,double bottomWidth,double depth,double length,
                               double angleDegrees,DovetailGrooveCommand* command,QString* error = nullptr) const;
    static DovetailGroovePreview buildDovetailGroovePreview(const DovetailGrooveCommand& command);
    bool commitDovetailGroove(const DovetailGrooveCommand& command,const TopoDS_Shape& preview,
                              QString* error = nullptr);
    bool prepareTSlotGroove(const QString& id,double centerX,double centerY,double entryZ,
                            double neckWidth,double undercutWidth,double depth,double undercutHeight,
                            double length,double angleDegrees,TSlotGrooveCommand* command,
                            QString* error = nullptr) const;
    static TSlotGroovePreview buildTSlotGroovePreview(const TSlotGrooveCommand& command);
    bool commitTSlotGroove(const TSlotGrooveCommand& command,const TopoDS_Shape& preview,
                           QString* error = nullptr);
    bool prepareRoundedSlot(const QString& id,double centerX,double centerY,double entryZ,
                            double width,double length,double depth,double angleDegrees,
                            RoundedSlotCommand* command,QString* error = nullptr) const;
    static RoundedSlotPreview buildRoundedSlotPreview(const RoundedSlotCommand& command);
    bool commitRoundedSlot(const RoundedSlotCommand& command,const TopoDS_Shape& preview,
                           QString* error = nullptr);
    bool prepareKeyholeSlot(const QString& id,double centerX,double centerY,double entryZ,
                            double headDiameter,double throatWidth,double throatLength,double depth,
                            double angleDegrees,KeyholeSlotCommand* command,QString* error = nullptr) const;
    static KeyholeSlotPreview buildKeyholeSlotPreview(const KeyholeSlotCommand& command);
    bool commitKeyholeSlot(const KeyholeSlotCommand& command,const TopoDS_Shape& preview,
                           QString* error = nullptr);
    bool prepareRoundedSlotPattern(const QString& id,double centerX,double centerY,double entryZ,
                                   double width,double length,double depth,double angleDegrees,
                                   int instanceCount,double spacingX,double spacingY,
                                   RoundedSlotPatternCommand* command,QString* error = nullptr) const;
    static RoundedSlotPatternPreview buildRoundedSlotPatternPreview(const RoundedSlotPatternCommand& command);
    bool commitRoundedSlotPattern(const RoundedSlotPatternCommand& command,const TopoDS_Shape& preview,
                                  QString* error = nullptr);
    bool holePattern(const QString& id,double x,double y,double entryZ,double diameter,double depth,bool throughAll,
                     int countX,double spacingX,int countY,double spacingY,QString* error = nullptr);
    bool circularHolePattern(const QString& id,double centerX,double centerY,double entryZ,double boltCircleRadius,
                             double diameter,double depth,bool throughAll,int instanceCount,double totalAngleDegrees,
                             QString* error = nullptr);
    bool circularCountersinkPattern(const QString& id,double centerX,double centerY,double entryZ,double boltCircleRadius,
                                    double diameter,double depth,bool throughAll,double headDiameter,
                                    double includedAngleDegrees,int instanceCount,double totalAngleDegrees,
                                    QString* error = nullptr);
    bool circularCounterborePattern(const QString& id,double centerX,double centerY,double entryZ,double boltCircleRadius,
                                    double diameter,double depth,bool throughAll,double headDiameter,double headDepth,
                                    int instanceCount,double totalAngleDegrees,QString* error = nullptr);
    bool linearCounterborePattern(const QString& id,double centerX,double centerY,double entryZ,double diameter,
                                  double depth,bool throughAll,double headDiameter,double headDepth,
                                  int instanceCount,double spacingX,double spacingY,QString* error = nullptr);
    bool linearCountersinkPattern(const QString& id,double centerX,double centerY,double entryZ,double diameter,
                                  double depth,bool throughAll,double headDiameter,double includedAngleDegrees,
                                  int instanceCount,double spacingX,double spacingY,QString* error = nullptr);
    bool linearKeyholeSlotPattern(const QString& id,double centerX,double centerY,double entryZ,double headDiameter,
                                  double throatWidth,double throatLength,double depth,double angleDegrees,
                                  int instanceCount,double spacingX,double spacingY,QString* error = nullptr);
    bool circularPattern(const QString& id, Axis axis, double centerU, double centerV,
                         int instanceCount, double totalAngleDegrees, QString* error = nullptr);
    bool circularPatternAboutAxis(const QString& id,double originX,double originY,double originZ,
                                  double axisX,double axisY,double axisZ,int instanceCount,
                                  double totalAngleDegrees,QString* error = nullptr);
    bool splitBody(const QString& id, Axis axis, double planeOffset, QString* error = nullptr);
    bool shellBody(const QString& id, Axis openingAxis, bool positiveSide,
                   double thickness, QString* error = nullptr);
    bool shellBodyWithOpenings(const QString& id,const QVector<ShellOpeningFace>& openings,
                               double thickness,QString* error = nullptr);
    bool rename(const QString& id, const QString& name, QString* error = nullptr);
    bool setVisibility(const QString& id, bool visible, QString* error = nullptr);
    bool setColor(const QString& id, const QString& hexColor, QString* error = nullptr);
    bool setMaterial(const QString& id,const QString& materialName,double densityKgPerM3,
                     QString* error = nullptr);
    bool setMaterial(const QStringList& ids,const QString& materialName,double densityKgPerM3,
                     QString* error = nullptr);
    bool erase(const QString& id, QString* error = nullptr);
    bool eraseMany(const QStringList& ids, QString* error = nullptr);
    void clear();
    void newDocument();
    bool undo();
    bool redo();
    bool canUndo() const noexcept { return !undo_.isEmpty(); }
    bool canRedo() const noexcept { return !redo_.isEmpty(); }
    bool saveAs(const QString& path, QString* error = nullptr);
    bool load(const QString& path, QString* error = nullptr);
    bool importSTEP(const QString& path, QString* error = nullptr);
    bool importIGES(const QString& path,QString* error = nullptr);
    bool importOBJ(const QString& path,double weldToleranceMm = 0.01,QString* error = nullptr);
    bool importSTL(const QString& path,double weldToleranceMm = 0.01,QString* error = nullptr);
    bool exportSTEP(const QString& path, QString* error = nullptr) const;
    bool exportIGES(const QString& path,QString* error = nullptr) const;
    bool exportSTL(const QString& path, QString* error = nullptr) const;
    bool exportBodyScheduleCsv(const QString& path, QString* error = nullptr) const;
    bool setPartMetadata(const QString& id, const QString& partNumber, const QString& description,
                         const QString& revision, QString* error = nullptr);
    bool setPartMetadata(const QString& id, const QString& partNumber, const QString& description,
                         const QString& revision, const QString& assemblyPath, QString* error = nullptr);
    bool setPartMetadata(const QString& id, const QString& partNumber, const QString& description,
                         const QString& revision, const QString& assemblyPath, const QString& procurementType,
                         const QString& supplier, const QString& supplierPartNumber, QString* error = nullptr);
    QVector<BillOfMaterialsRow> billOfMaterials(QString* error = nullptr) const;
    QVector<BillOfMaterialsRow> billOfMaterials(const QStringList& bodyIds, QString* error = nullptr) const;
    QVector<PartsListRow> partsList(QString* error = nullptr) const;
    bool exportBillOfMaterialsCsv(const QString& path, QString* error = nullptr) const;
    bool exportPartsListCsv(const QString& path, QString* error = nullptr) const;
    StandardsCheckReport checkStandards(StandardsProfile profile) const;

private:
    struct Snapshot { QVector<Part> parts; DrawingReadinessPolicy drawingReadinessPolicy; quint64 revision; };
    QVector<Part> parts_;
    DrawingReadinessPolicy drawingReadinessPolicy_;
    QVector<Snapshot> undo_;
    QVector<Snapshot> redo_;
    QString path_;
    quint64 revision_ = 0;
    quint64 savedRevision_ = 0;
    quint64 nextRevision_ = 1;
    void checkpoint();
    QString uniqueName(const QString& base) const;
    static bool validShape(const TopoDS_Shape& shape, QString* error);
    bool addSweepProfile(bool circular,bool elliptical,double width,double height,double pathLength,double lateralOffset,
                         double verticalRise,QString* error);
    static void setError(QString* error, const QString& text);
};
} // namespace cad
