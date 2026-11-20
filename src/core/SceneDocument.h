#pragma once

#include "SheetMetal.h"
#include "StockSheetLayout.h"
#include "StockContourNesting.h"
#include "SketchSolver.h"
#include "SketchProfile.h"
#include "MassTensorAnalysis.h"
#include "AssemblyMateSolver.h"
#include "AssemblyConstraintNetwork.h"
#include "ArticulatedAssemblyNetwork.h"

#include <QString>
#include <QByteArray>
#include <QStringList>
#include <QMap>
#include <QPointF>
#include <QUuid>
#include <QVector>
#include <vector>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>

namespace cad {

struct DrawingNoteAnnotation {
    QString id;
    QString text;
    double xMm = 12.0;
    double yMm = 180.0;
    bool operator==(const DrawingNoteAnnotation& other) const = default;
};

struct DrawingDimensionAnnotation {
    QString id;
    QString axis;
    double offsetMm = 9.0;
    bool operator==(const DrawingDimensionAnnotation& other) const = default;
};

struct DrawingEdgeDimensionAnnotation {
    QString id;
    QString sourceBodyId;
    QString sourceGeometryToken;
    int edgeIndex = 0;
    QString view = QStringLiteral("TOP");
    double offsetMm = 9.0;
    // Versioned geometric witness; empty for old, exact-revision-only sheets.
    // Stored independently from an edge traversal index, which is not stable.
    QMap<QString,double> edgeAnchor;
    // LENGTH, DIAMETER (one full circle), or CENTER_DISTANCE (two coplanar
    // full circles). Both geometric references are re-resolved on export.
    QString kind = QStringLiteral("LENGTH");
    int secondEdgeIndex = 0;
    QMap<QString,double> secondEdgeAnchor;
    // User-supplied bilateral drawing tolerances, not inferred from geometry.
    double upperToleranceMm = 0.0;
    double lowerToleranceMm = 0.0;
    bool operator==(const DrawingEdgeDimensionAnnotation& other) const = default;
};

// Explicitly authored, non-certified geometric-tolerance frame. The frame
// is attached to a source *body* revision (not a measured feature or a formal
// datum system). A changed B-rep requires the engineer to review/re-author it.
struct DrawingGdtFrameAnnotation {
    QString id;
    QString sourceBodyId;
    QString sourceGeometryToken;
    QString characteristic = QStringLiteral("POSITION");
    double toleranceMm = 0.1;
    QString datumA,datumB,datumC;
    double xMm = 12.0,yMm = 155.0;
    bool operator==(const DrawingGdtFrameAnnotation& other) const = default;
};

struct DrawingPartsListAnnotation {
    QString id;
    bool visible = false;
    double xMm = 12.0;
    double yMm = 12.0;
    bool operator==(const DrawingPartsListAnnotation& other) const = default;
};

struct DrawingBalloonAnnotation {
    QString id;
    QString sourceBodyId;
    double xMm = 250.0;
    double yMm = 170.0;
    bool operator==(const DrawingBalloonAnnotation& other) const = default;
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
    bool operator==(const DrawingDetailAnnotation& other) const = default;
};

struct DrawingSectionAnnotation {
    QString id;
    QString planeAxis = QStringLiteral("Y");
    double depthFraction = 0.5;
    double xMm = 240.0;
    double yMm = 145.0;
    bool operator==(const DrawingSectionAnnotation& other) const = default;
};

struct DrawingSheetSettings {
    // Persistent sheet/view identities and editable projected-view centers (paper mm).
    QString sheetId;
    QString sourceGeometryToken;
    // Separate from geometry: linked BOM rows can become stale when only part
    // metadata or assigned material changes.
    QString sourceDataToken;
    QStringList sourceBodyIds;
    QMap<QString,QString> viewIds;
    QMap<QString,QPointF> viewCenters;
    QVector<DrawingNoteAnnotation> noteAnnotations;
    QVector<DrawingDimensionAnnotation> dimensionAnnotations;
    QVector<DrawingEdgeDimensionAnnotation> edgeDimensionAnnotations;
    QVector<DrawingGdtFrameAnnotation> gdtFrames;
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
    bool operator==(const DrawingSheetSettings& other) const = default;
};

struct JointMotionKeyframe {
    double timeSeconds = 0.0;
    double value = 0.0; // degrees for revolute joints, millimetres for prismatic joints
};

// Optional values are not engineering-certified; zero modulus means unknown.
struct MaterialEngineeringData {
    QString provenance;
    double youngsModulusGPa = 0.0;
};

struct MaterialLibraryEntry {
    QString name;
    double densityKgPerM3 = 0.0;
    MaterialEngineeringData engineering;
};

// A directed, dimension-only dependency: target = source * scale + offset.
struct ParameterLink {
    QString source;
    double scale = 1.0;
    double offset = 0.0;
};

// An ordered, stable-ID modifier stack attached to any B-rep body, including
// imported parts and bodies created by non-parametric tools. Each node operates
// on the preceding result; the frozen base is retained for deterministic edits.
struct ShapeFeatureNode {
    QString id;
    QString type;
    QMap<QString,double> parameters;
    bool suppressed = false;
    // Empty means legacy sequential input. Explicit IDs point to earlier nodes;
    // "@base" points to the frozen body. Two inputs are used by Boolean nodes.
    QStringList inputs;
};

// A bounded associative mixed feature DAG. Empty inputs preserve sequential
// v1 histories, one input selects an earlier feature or @base, and two
// inputs are permitted for Shape Boolean branch combiners. The last step
// supplies the visible result. External sketch sources are separate bodies.
struct InterleavedHistoryStep {
    QString id;
    QString kind; // "Shape", "SketchFuse", "SketchCut"
    ShapeFeatureNode feature;
    QString sketchSourceId;
    double depthMm=0, offsetMm=0;
    QStringList inputs; // Earlier step IDs or @base, max 2; no forward references.
    // Optional Boolean-parent source branch for persistent edge/face anchors.
    // Must reference one direct input of the selected Boolean parent stage.
    QString referenceSourceId;
    // 0 = original pattern instance; positive indices select the exact
    // occurrence in a single tracked pattern stage (v4 saved projects).
    int referencePatternOccurrence=0; // Legacy v4: exactly one pattern hop.
    // v5: explicit instance for EACH tracked pattern stage (ID -> zero-based
    // occurrence). Only non-seed selections are persisted; omitted means seed.
    // Entries must be ancestors on every accepted route, not positional guesses.
    QMap<QString,int> referencePatternOccurrences;
};

// An authored, named design variant. Expressions use the same unit-aware
// evaluator as the configuration study; no B-rep is serialized in a preset.
struct SavedDesignConfiguration {
    QString name;
    QMap<QString,QString> expressions;
    bool operator==(const SavedDesignConfiguration& other) const = default;
};

// Read-only fully rebuilt configuration-versus-BOM design study. Every row
// derives from a disposable scene with a fresh geometry rebuild (not merely
// an expression preview), with a common baseline and no document mutation.
struct ConfigurationBomComparison {
    QString name;
    bool current = false;
    double targetVolumeMm3 = 0.0;
    double totalAssemblyVolumeMm3 = 0.0;
    double knownAssemblyMassKg = 0.0;
    bool allMassesKnown = false;
    int bomLines = 0;
    int componentQuantity = 0;
    bool operator==(const ConfigurationBomComparison& other) const = default;
};

// Independent what-if candidates are rebuilt in detached full documents. A
// rejected alternative reports its reason without invalidating other choices.
// Optional rectangular stock-layout planning for an associative multi-part
// manufacturing job. This is an envelope packing estimate, not CNC nesting.
struct StockLayoutSettings {
    bool enabled=false;
    double stockWidthMm=0.0,stockHeightMm=0.0,gapMm=0.0;
    bool allowRotation=false;
    // Opt-in API flag; the interactive stock-job editor enables it by default.
    // For an existing stock layout with mixed gauges, keep the previous
    // build/export behavior only when this is intentionally disabled.
    bool requireCompatibleGaugeAndMaterial=false;
};

struct ParametricDesignAlternative {
    QString label;
    QMap<QString,QMap<QString,double>> changes;
};
// Per-body B-rep consequences of a detached design-alternative rebuild.
// Different face/edge counts signal changed topology, but equal counts do not
// prove stable topological identity across face splits or merges.
struct ParametricBodyImpact {
    QString bodyId,bodyName;
    double beforeVolumeMm3=0.0,afterVolumeMm3=0.0;
    int beforeFaces=0,afterFaces=0,beforeEdges=0,afterEdges=0;
    bool topologyCountChanged=false;
};
struct ParametricDesignAlternativeResult {
    QString label;
    bool valid = false;
    QString failure;
    int rebuiltBodies = 0;
    int topologyCountChangedBodies = 0;
    QVector<ParametricBodyImpact> bodyImpacts;
    double totalSolidVolumeMm3 = 0.0;
    double volumeChangePercent = 0.0;
    double knownMassKg = 0.0;
};

// Stable-ID engineering intent attached to a feature-history node, not to an
// arbitrary face index. Stored revision is intentionally NOT refreshed by a
// geometry edit; the annotation must be reviewed again before issue.
struct FeatureInspectionNote {
    QString id;
    QString featureId;
    QString category; // DESIGN, INSPECTION, PROCESS
    QString text;
    QString reviewedGeometryToken;
    bool operator==(const FeatureInspectionNote& other) const = default;
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
    MaterialEngineeringData materialEngineering;
    QString partNumber;
    QString description;
    QString revision;
    QString assemblyPath;
    QString procurementType = QStringLiteral("Manufactured");
    QString supplier;
    QString supplierPartNumber;
    // Persistent assembly identity. Bodies with the same definition ID are reusable component occurrences.
    QString componentDefinitionId;
    bool hasFoldedSheetMetalDefinition = false;
    QString foldedSheetGeometryToken;
    QVector<double> foldedSheetFlangesMm;
    double foldedSheetWidthMm = 0.0;
    double foldedSheetThicknessMm = 0.0;
    QVector<FoldedSheetBend> foldedSheetBends;
    // Associated folded development. Empty K factors preserve the historical
    // uniform K=0.33 default. Optional rectangular bend-end reliefs are cut
    // into the linked 3-D solid at the same stations as DXF/SVG exports.
    QString linkedFoldedFlatSourceId;
    QString linkedFoldedFlatSourceToken;
    QVector<double> linkedFoldedFlatKFactors;
    SheetMetalRelief linkedFoldedFlatRelief;
    QString componentName;
    QString occurrenceId;
    double placementX = 0.0;
    double placementY = 0.0;
    double placementZ = 0.0;
    // Optional v19 placement extension for free occurrences only; about world Z.
    double placementYawDegrees = 0.0;
    double placementPitchDegrees = 0.0;
    double placementRollDegrees = 0.0;
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
    // General shape-independent modifier stack. Its stored B-rep is the exact
    // input to node zero, not necessarily one of the built-in primitive types.
    bool hasShapeFeatureTree = false;
    TopoDS_Shape shapeFeatureBase;
    QString shapeFeatureGeometryToken;
    QVector<ShapeFeatureNode> shapeFeatureNodes;
    // Transient OCCT feature outputs in node order. Not serialized: these are
    // derived B-reps, rebuilt on load and invalidated with the source body.
    // Shared OCCT shape handles make undo/redo snapshots inexpensive.
    QVector<TopoDS_Shape> shapeFeatureCachedOutputs;
    // Separate v1.70 history path for associative interleaved feature DAGs. The
    // older two-layer shape/sketch stacks remain unchanged and readable.
    bool hasInterleavedHistory=false;
    TopoDS_Shape interleavedBase;
    QString interleavedGeometryToken;
    QVector<InterleavedHistoryStep> interleavedSteps;
    QStringList interleavedSourceTokens;
    // Focused editable history for a constrained rectangular XY sketch + extrusion.
    bool hasParametricDefinition = false;
    QString parametricGeometryToken;
    QMap<QString,double> namedParameters;
    QMap<QString,QString> namedParameterFormulas; // Persisted, unit-checked dimension expressions.
    QVector<SavedDesignConfiguration> designConfigurations;
    // Audit label for the most recently *applied* stored preset. Manual edits
    // clear this name without deleting the reusable configuration definitions.
    QString activeDesignConfiguration;
    QVector<FeatureInspectionNote> featureInspectionNotes;
    QMap<QString,ParameterLink> parameterLinks;
    QStringList sketchConstraints;
    QStringList featureHistory;
    // Stable operation keys for the optional features hidden from the rebuilt result.
    QStringList suppressedParametricFeatures;
    QString holePatternAxis = QStringLiteral("X");
    bool hasConstrainedPolygonDefinition = false;
    SketchModel constrainedPolygonSketch;
    QStringList constrainedPolygonBoundary;
    double constrainedPolygonDepthMm = 0.0;
    SketchPlane constrainedPolygonPlane;
    QString constrainedPolygonGeometryToken;
    // True for a persisted surface-only face/shell body. Solid modeling
    // features continue to require a solid and reject these bodies.
    bool isSurfaceBody = false;
    // Additive v24 extension: frozen source IDs and revision tokens for a
    // separately selectable associative Boolean result. Source bodies remain.
    bool hasLinkedBoolean = false;
    QString linkedPrimaryId, linkedToolId, linkedOperation;
    QString linkedPrimaryToken, linkedToolToken;
    // Single-source associative extrusion. The source owns the original
    // constrained sketch; geometry is rebuilt from that sketch on every
    // source change. No sketch geometry is copied into this dependent body.
    bool hasLinkedSketchExtrusion = false;
    QString linkedSketchSourceId, linkedSketchSourceToken;
    double linkedSketchDepthMm = 0.0, linkedSketchOffsetMm = 0.0;
    // A source-sketch boss/pocket applied to THIS existing body ID. Its base
    // B-rep is frozen before the operation; no duplicate result body is made.
    // The dependency graph contains the external sketch source as a unary edge.
    bool hasInPlaceSketchFeature = false;
    QString inPlaceSketchSourceId, inPlaceSketchSourceToken, inPlaceSketchOperation;
    double inPlaceSketchDepthMm = 0.0, inPlaceSketchOffsetMm = 0.0;
    TopoDS_Shape inPlaceSketchBase;
    // Optional second ordered modifier. Both source IDs are dependency edges;
    // the second operation acts on the first operation's evaluated solid.
    bool hasInPlaceSketchFollowUp = false;
    QString inPlaceFollowSourceId, inPlaceFollowSourceToken, inPlaceFollowOperation;
    double inPlaceFollowDepthMm = 0.0, inPlaceFollowOffsetMm = 0.0;
    // Stages 3..8. The first two historical fields remain unchanged for v24 scenes.
    struct AdditionalInPlaceSketchStage {
        QString sourceId, sourceToken, operation;
        double depthMm=0.0, offsetMm=0.0;
    };
    QVector<AdditionalInPlaceSketchStage> extraInPlaceSketchStages;
};

enum class DrawingSourceState { NoSavedSheet, Current, Stale, MissingBodies };
struct DrawingSheetSourceStatus {
    DrawingSourceState state = DrawingSourceState::NoSavedSheet;
    QStringList sourceBodyIds;
    QStringList missingSourceBodyIds;
    // Edge dimensions and manually authored GD&T frames retain their own
    // captured revisions. Refreshing the whole sheet must not approve them.
    QStringList staleAnnotationIds;
    QString storedGeometryToken;
    QString currentGeometryToken;
    QString storedDataToken;
    QString currentDataToken;
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
    MaterialEngineeringData materialEngineering;
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
    MaterialEngineeringData materialEngineering;
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

// Summary of the cleanup applied to an imported triangle mesh. It is written
// only after the solid import succeeds, so callers retain prior values on error.
struct MeshImportSummary {
    quint64 inputVertices = 0;
    quint64 outputVertices = 0;
    quint64 inputTriangles = 0;
    quint64 outputTriangles = 0;
    quint64 weldedVertices = 0;
    quint64 removedDegenerateTriangles = 0;
    quint64 removedDuplicateTriangles = 0;
    quint64 reorientedTriangles = 0;
    quint64 filledHoles = 0;
    quint64 filledHoleTriangles = 0;
    quint64 boundaryEdges = 0;
    quint64 nonManifoldEdges = 0;
    bool importedAsSurfaceBody = false;
    bool operator==(const MeshImportSummary& other) const = default;
};

struct InterferenceResult {
    double volume = 0.0;
    double centroidX = 0.0;
    double centroidY = 0.0;
    double centroidZ = 0.0;
};

struct MotionInterferenceSample {
    double timeSeconds = 0.0;
    double overlapVolumeMm3 = 0.0;
    bool interfering = false;
    // Exact closest-body distance at this sampled pose; zero at contact or overlap.
    double clearanceMm = 0.0;
    double firstX = 0.0, firstY = 0.0, firstZ = 0.0;
    double secondX = 0.0, secondY = 0.0, secondZ = 0.0;
    bool operator==(const MotionInterferenceSample& other) const = default;
};

struct MotionClearanceInterval {
    double startSeconds = 0.0;
    double endSeconds = 0.0;
    double minimumEndpointClearanceMm = 0.0;
    double clearanceLowerBoundMm = 0.0;
    bool speedBoundAvailable = false;
    bool sampledOverlap = false;
    bool provenAboveMargin = false;
    bool refinementNeeded = false;
    bool operator==(const MotionClearanceInterval& other) const = default;
};

struct MotionInterferenceContact {
    double timeSeconds = 0.0;
    QString firstBodyId;
    QString secondBodyId;
    double overlapVolumeMm3 = 0.0;
    bool operator==(const MotionInterferenceContact& other) const = default;
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
enum class ExtrusionSketchPlane { XY, XZ, YZ, Custom };
enum class ExtrusionMode { NewBody, Join, Cut };

struct ExtrusionCommand {
    SketchProfile profile = SketchProfile::Rectangle;
    ExtrusionSketchPlane sketchPlane = ExtrusionSketchPlane::XY;
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
    gp_Dir inwardAxis = gp_Dir(0,0,-1); // Explicit drill direction, default world -Z.
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
struct LinkedBooleanEdit {
    QString resultId;
    BooleanOp operation = BooleanOp::Fuse;
    QString primaryId;
    QString toolId;
};
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
    bool addSurfaceBody(const QString& name,const TopoDS_Shape& shape,QString* error = nullptr);
    bool thickenSurfaceBody(const QString& surfaceBodyId,double thicknessMm,QString* error = nullptr);
    bool stitchSurfaceBodies(const QStringList& surfaceBodyIds,double toleranceMm,QString* error = nullptr);
    bool sewSurfaceBodies(const QStringList& surfaceBodyIds,double toleranceMm,QString* error = nullptr);
    bool trimSurfaceBody(const QString& surfaceBodyId,const QString& cutterSolidId,
                         bool keepInsideCutter,QString* error = nullptr);
    bool splitSurfaceBodyBySolid(const QString& surfaceBodyId,const QString& cutterSolidId,
                                 QString* error = nullptr);
    bool addFoldedSheetMetal(const QVector<double>& straightFlangeLengthsMm,double sheetWidthMm,
                             double thicknessMm,const QVector<FoldedSheetBend>& bends,
                             QString* error = nullptr);
    bool foldedSheetMetalDefinition(const QString& partId,QVector<double>* straightFlangeLengthsMm,
                                    double* sheetWidthMm,double* thicknessMm,
                                    QVector<FoldedSheetBend>* bends,bool* current = nullptr) const;
    // Read-only refold geometry audit against the PERSISTED actual OCCT solid.
    // Rebuilds the faceted cross-section independently, checks mid-surface
    // development/volume conservation and verifies that the stored B-rep
    // agrees with a fresh folded solid. All output is unissued engineering QA.
    bool auditFoldedSheetRefoldGeometry(const QString& partId,
        QByteArray* csv,QString* error=nullptr) const;
    // Read-only OCCT rebuild plus neutral flat-length check before a folded edit.
    // Does not update tokens, drawings or undo history; outputs are atomic.
    bool previewFoldedSheetMetalEdit(const QString& partId,const QVector<double>& flanges,
                                    double width,double thickness,
                                    const QVector<FoldedSheetBend>& bends,
                                    double* solidVolumeMm3,double* neutralFlatLengthMm,
                                    QString* error=nullptr) const;
    bool addLinkedFoldedFlatBlank(const QString& foldedBodyId,QString* error=nullptr);
    bool addLinkedFoldedFlatBlank(const QString& foldedBodyId,const QVector<double>& kFactors,
                                  const SheetMetalRelief& relief,QString* error=nullptr);
    // Edit derived development dimensions without changing its stable ID.
    // Validates the exact solid before a single undoable commit.
    bool editLinkedFoldedFlatDevelopment(const QString& flatBodyId,
                                         const QVector<double>& kFactors,
                                         const SheetMetalRelief& relief,QString* error=nullptr);
    bool editFoldedSheetMetal(const QString& partId,const QVector<double>& straightFlangeLengthsMm,
                              double sheetWidthMm,double thicknessMm,const QVector<FoldedSheetBend>& bends,
                              QString* error = nullptr);
    // Structural flange-count edits can supply new development K-factors for
    // each linked blank in the same transaction. Every changed flat blank is
    // regenerated from the proposed folded source; malformed mapping, relief
    // geometry or an invalid solid rolls the entire scene back.
    bool editFoldedSheetMetalWithDevelopments(const QString& partId,
        const QVector<double>& straightFlangeLengthsMm,double sheetWidthMm,
        double thicknessMm,const QVector<FoldedSheetBend>& bends,
        const QMap<QString,QVector<double>>& linkedKFactors,QString* error=nullptr);
    // Single-source, read-only associative development bundle. All three
    // outputs commit together only after SVG, DXF, CSV and bend-station
    // agreement pass. Stale or missing source geometries fail atomically.
    bool buildLinkedFlatDevelopmentSet(const QString& flatBodyId,
        QByteArray* svg,QByteArray* dxf,QByteArray* bendCsv,QString* error=nullptr) const;
    // Export one self-contained, hash-checked manufacturing package directory:
    // SVG, DXF, bend CSV and a JSON provenance manifest. A temporary sibling
    // directory is renamed only after all four files are written successfully.
    bool exportLinkedFlatManufacturingPackage(const QString& flatBodyId,
            const QString& newDirectory,QString* error=nullptr) const;
    // Batch-export 2-24 linked blanks as a single manufactured job. Every
    // source is validated before any files are staged; only a complete package
    // is published. Existing output directories are never overwritten.
    bool exportLinkedFlatManufacturingBatch(const QStringList& flatBodyIds,
            const QString& newDirectory,QString* error=nullptr,
            const StockLayoutSettings& stock={}) const;
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
    // Absolute world-Z yaw about the occurrence's XYZ pivot; no joints/descendants.
    bool setFreeOccurrenceYaw(const QString& bodyId,double degrees,QString* error = nullptr);
    bool setFreeOccurrenceOrientation(const QString& bodyId,double rollDegrees,double pitchDegrees,
                                      double yawDegrees,QString* error = nullptr);
    bool synchronizeComponentOccurrences(const QString& sourceBodyId,QString* error = nullptr);
    bool linearPatternComponentOccurrences(const QString& sourceBodyId,int instanceCount,
                                          double spacingX,double spacingY,double spacingZ,
                                          QString* error = nullptr);
    bool circularPatternComponentOccurrences(const QString& sourceBodyId,int instanceCount,
                                             double centerX,double centerY,double centerZ,
                                             double angleStepDegrees,QString* error = nullptr);
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
    // Solve six-DOF point/coaxial/planar mates for a free child occurrence.
    // An optional Rigid joint persists the solved placement and child-parent
    // hierarchy. Raw constraint observations are not yet stored as live mates.
    bool solveAssemblyMates(const QString& parentBodyId,const QString& childBodyId,
                            const std::vector<mates::Constraint>& constraints,
                            bool attachRigidJoint,QString* error=nullptr);
    struct AssemblyNetworkMate {
        QString parentBodyId, childBodyId;
        std::vector<mates::Constraint> constraints;
    };
    // Solve an arbitrary connected graph of fully constraining rigid mates.
    // Supports consistent closed loops; moves all free component occurrences in
    // one undo entry. A rejected graph never updates a body or history record.
    // The resulting placements persist; raw constraints are not live on reopen.
    bool solveAssemblyMateNetwork(const QString& fixedBodyId,
                                  const QVector<AssemblyNetworkMate>& connections,
                                  QString* error=nullptr);
    // Editable *local-frame* mate definitions. Unlike solved placements, these
    // observations survive scene save/reopen and can be explicitly re-solved
    // after permitted component placement edits. A failed refresh is atomic.
    bool setLiveRigidMateNetwork(const QString& fixedBodyId,
                                 const QVector<AssemblyNetworkMate>& worldConnections,
                                 QString* error=nullptr);
    // Extend the persisted connected graph with a new world-frame connection.
    // Existing local definitions are reprojected from the CURRENT occurrence
    // placements; rejected additions never change the stored graph or scene.
    bool appendLiveRigidMateConnection(const AssemblyNetworkMate& worldConnection,
                                       QString* error=nullptr);
    // Read-only projection of the persisted component-local observations to
    // world coordinates at the CURRENT placements, for GUI re-authoring.
    // The output remains untouched on failure.
    bool liveRigidMateWorldDefinitions(QVector<AssemblyNetworkMate>* world,
                                       QString* error=nullptr) const;
    // Replace one connection in-place (same occurrence pair and index).
    // Validate the ENTIRE solved graph and commit updated local-frame mate
    // observations plus moved B-reps in one undoable transaction.
    bool replaceLiveRigidMateConnection(int index,const AssemblyNetworkMate& worldConnection,
                                        QString* error=nullptr);
    // Read-only network and transformed B-rep preview. Checks exact nominal
    // pair clearance for at most 12 network bodies; larger graphs explicitly
    // report skipped fit verification. Not a swept-motion or tolerance proof.
    // A failed request leaves the output string and document unchanged.
    bool previewLiveRigidMateConnectionEdit(int index,const AssemblyNetworkMate& worldConnection,
                                            QString* report,QString* error=nullptr,
                                            double requiredClearanceMm=0.0) const;
    // Remove an individual connection only if the remaining saved graph is
    // still a connected, uniquely solvable fully constrained network.
    bool removeLiveRigidMateConnection(int index,QString* error=nullptr);
    bool refreshLiveRigidMateNetwork(QString* error=nullptr);
    bool removeLiveRigidMateNetwork();
    bool hasLiveRigidMateNetwork() const noexcept { return !liveRigidMateFixed_.isEmpty(); }
    QString liveRigidMateFixedBodyId() const { return liveRigidMateFixed_; }
    QVector<AssemblyNetworkMate> liveRigidMateDefinitions() const { return liveRigidMates_; }
    struct ArticulatedNetworkJoint {
        QString parentBodyId,childBodyId;
        articulated::JointKind kind=articulated::JointKind::Fixed;
        mates::Transform childAtZero;
        mates::Vec3 axis{0,0,1};
        double initial=0,minimum=0,maximum=0;
        bool driven=true;
    };
    // Solve joint coordinates and closed-loop component poses as one scene
    // transaction. Zero frames are corrections relative to the present world
    // placements; the fixed occurrence remains unmoved. Independent live mate
    // authoring/persistence is not yet part of this API.
    bool solveArticulatedAssemblyNetwork(const QString& fixedBodyId,
                                         const QVector<ArticulatedNetworkJoint>& joints,
                                         QVector<double>* solvedCoordinates=nullptr,
                                         QString* error=nullptr);
    // Persist an articulated loop in a fixed reference pose. Each saved
    // joint retains its limits and the original zero-reference placement.
    // Refresh solves absolute targets (rather than applying the previous
    // delta a second time), so repeated refreshes are idempotent.
    bool setLiveArticulatedNetwork(const QString& fixedBodyId,
                                   const QVector<ArticulatedNetworkJoint>& joints,
                                   QString* error=nullptr);
    bool refreshLiveArticulatedNetwork(QString* error=nullptr);
    // Solve and inspect a proposed driven coordinate against the actual staged
    // OCCT solids without changing the document, saved joints, undo history,
    // or caller report on failure. The pairwise check is a static pose check,
    // not a continuous motion/clearance certification.
    bool previewLiveArticulatedJointCoordinate(int jointIndex,double requestedCoordinate,
                                               QString* report,QString* error = nullptr,
                                               double requiredClearanceMm = 0.0) const;
    bool updateLiveArticulatedJointCoordinate(int jointIndex,double requestedCoordinate,
                                              QString* error = nullptr);
    // Stage an arbitrary set of driven joint targets, solve the complete loop
    // once and check all resulting solids. No partial coordinate commits.
    // Index keys refer to the saved joint order; units are radians or mm.
    bool previewLiveArticulatedCoordinates(const QMap<int,double>& targets,
                                           QString* report,QString* error=nullptr,
                                           double requiredClearanceMm=0.0,
                                           bool* hasUnsafeNominalPairs=nullptr) const;
    // Read-only sampled path between the current and proposed driven coordinates.
    // Every pose is independently solved and B-rep preflighted. When <=12
    // occurrences participate it also performs exact nominal pairwise fits at
    // EACH sampled pose. Unsampled intervals are not collision-certified.
    // Failure is atomic for the document AND caller's report.
    bool previewLiveArticulatedTrajectory(const QMap<int,double>& targets,
                                           int sampleCount,QString* report,
                                           QString* error=nullptr,
                                           double requiredClearanceMm=0.0,
                                           bool rejectUnsafeSamples=false,
                                           int refinementsPerInterval=1,
                                           bool easeInOut=false) const;
    // Complete sampled motion evidence for review in a spreadsheet. The same
    // independent solver / exact sampled-fit checks used by previews produce
    // each row. A rejected sample leaves the output bytes unchanged.
    bool buildLiveArticulatedMotionCsv(const QMap<int,double>& targets,
          int baseIntervals,int refinement,bool easeInOut,double requiredClearanceMm,
          bool rejectUnsafe,QByteArray* csv,QString* error=nullptr) const;
    // Independently regenerates every sampled articulated pose, accumulating
    // each moving occurrence's axis-aligned world-space bounding envelope.
    // Includes the stationary reference fixture in pair checks. Pure
    // prismatic/fixed networks additionally report translation-only inter-pose
    // AABB sweeps; rotating mechanisms are explicitly unsupported. This is
    // broad-phase evidence, NOT exact B-rep collision certification. Optional
    // structured outputs commit only if the entire read-only export succeeds.
    bool buildLiveArticulatedEnvelopeCsv(const QMap<int,double>& targets,
        int baseIntervals,int refinement,bool easeInOut,QByteArray* csv,
        QString* error=nullptr,bool* translationSweepSupported=nullptr,
        double* closestSweptBoxGapMm=nullptr) const;
    bool updateLiveArticulatedCoordinates(const QMap<int,double>& targets,
                                          QString* error=nullptr);
    // Enforces the selected nominal and sampled-motion fit policy at the
    // actual commit boundary, not just in a GUI preview. A rejection leaves
    // the graph, solids, undo history and saved drive values unchanged.
    // Set baseIntervals=0 to enforce the final pose only.
    bool commitLiveArticulatedCoordinatesChecked(const QMap<int,double>& targets,
        double requiredClearanceMm,bool rejectUnsafe,int baseIntervals,
        int refinement=1,bool smoothstep=false,QString* error=nullptr);
    // Edit a saved articulated connection's axis, zero-frame translation,
    // coordinate and limits while retaining its endpoints/kind and stable
    // index. A detached full-graph solve and OCCT placement rebuild must
    // succeed before committing the complete definition and pose as one undo.
    // The preview additionally inspects static nominal pairwise fit for <=12
    // bodies; it never mutates the document or caller report on failure.
    bool previewLiveArticulatedJointDefinitionEdit(int index,
        const ArticulatedNetworkJoint& candidate,QString* report,
        QString* error=nullptr,double requiredClearanceMm=0.0) const;
    bool replaceLiveArticulatedJointDefinition(int index,
        const ArticulatedNetworkJoint& candidate,QString* error=nullptr);
    // Sprint 15: edit several existing articulated mate definitions in one
    // graph solve, B-rep regeneration and undo checkpoint. No individual
    // intermediate joint pose can poison an otherwise valid final graph.
    // Strict mode rejects failed nominal fits and any uninspected >12-body
    // graph. Continuous swept-motion safety is NOT implied by this API.
    bool previewLiveArticulatedJointDefinitionBatch(
        const QMap<int,ArticulatedNetworkJoint>& edits,QString* report,
        QString* error=nullptr,double clearanceMm=0.0,
        bool rejectUnsafeNominal=false) const;
    bool replaceLiveArticulatedJointDefinitionBatch(
        const QMap<int,ArticulatedNetworkJoint>& edits,QString* error=nullptr,
        double clearanceMm=0.0,bool rejectUnsafeNominal=false);
    // Rebind one existing saved joint to two OTHER members of the very same
    // persisted articulated network. Preserves joint kind, axis, limits and
    // index; graph connectivity, loop closure and every staged B-rep are
    // validated afresh. A missing/foreign occurrence or an unsolvable graph
    // fails without changing either the scene or a caller-provided report.
    bool previewLiveArticulatedJointRebind(int index,
        const QString& newParentBodyId,const QString& newChildBodyId,
        QString* report,QString* error=nullptr,
        double requiredClearanceMm=0.0) const;
    bool rebindLiveArticulatedJoint(int index,
        const QString& newParentBodyId,const QString& newChildBodyId,
        QString* error=nullptr);
    bool removeLiveArticulatedNetwork();
    bool hasLiveArticulatedNetwork() const noexcept {return !liveArticulatedFixed_.isEmpty();}
    QString liveArticulatedFixedBodyId() const {return liveArticulatedFixed_;}
    QVector<ArticulatedNetworkJoint> liveArticulatedJointDefinitions() const {return liveArticulatedJoints_;}
    bool setJointMotionKeyframes(const QString& childBodyId,const QVector<JointMotionKeyframe>& keyframes,
                                 QString* error = nullptr);
    // Coordinated multi-joint timeline edit. Previews exact per-joint rates,
    // validates EVERY saved track before changing anything, and commits all
    // changed tracks in one undo checkpoint. It does not modify joint poses,
    // execute the motion, or certify collision-free motion.
    bool previewJointMotionKeyframesBatch(
        const QMap<QString,QVector<JointMotionKeyframe>>& tracks,
        QString* report,QString* error=nullptr) const;
    bool setJointMotionKeyframesBatch(
        const QMap<QString,QVector<JointMotionKeyframe>>& tracks,
        QString* error=nullptr);

    bool evaluateAssemblyMotionStudy(double timeSeconds,QString* error = nullptr);
    bool scanAssemblyMotionInterference(const QString& firstId,const QString& secondId,
                                        double startSeconds,double endSeconds,int sampleCount,
                                        QVector<MotionInterferenceSample>* samples,QString* error = nullptr) const;
    // Bounded adaptive pair scan. Includes all saved keyframe times; refines
    // dangerous and long intervals without modifying the document. This is
    // sampled evidence, NOT a continuous collision guarantee unless an
    // independently justified surface-speed bound is supplied. Even then the
    // result is conditional on the supplied bound being valid. On failure
    // all outputs remain unchanged.
    bool scanAssemblyMotionAdaptive(const QString& firstId,const QString& secondId,
                                    double startSeconds,double endSeconds,int maximumSamples,
                                    double safetyClearanceMm,double coarseStepSeconds,
                                    double fineStepSeconds,
                                    QVector<MotionInterferenceSample>* samples,
                                    bool* unresolvedIntervals,QString* error=nullptr,
                                    double provenRelativeSpeedBoundMmPerSecond=-1.0,
                                    bool* clearanceBoundSatisfied=nullptr,
                                    double* conservativeClearanceLowerBoundMm=nullptr,
                                    QVector<MotionClearanceInterval>* intervalAudit=nullptr) const;
    // Returns only pairs with positive sampled overlap. Scanning is read-only
    // and limited to 20 solid bodies and 51 time samples.
    bool scanAssemblyMotionAllPairs(double startSeconds,double endSeconds,int sampleCount,
                                    QVector<MotionInterferenceContact>* contacts,QString* error = nullptr) const;
    bool exportMotionContactsCsv(const QVector<MotionInterferenceContact>& contacts,
                                 const QString& path,QString* error = nullptr) const;
    bool exportMotionSamplesCsv(const QString& firstBodyId,const QString& secondBodyId,
                                const QVector<MotionInterferenceSample>& samples,
                                const QString& path,QString* error = nullptr) const;
    bool exportMotionClearanceIntervalsCsv(const QString& firstBodyId,const QString& secondBodyId,
                                           const QVector<MotionClearanceInterval>& intervals,
                                           const QString& path,QString* error = nullptr) const;
    // Resolve defaults and validate saved view/annotation links without an undo checkpoint.
    bool prepareDrawingSheetSettings(const QString& partId,const DrawingSheetSettings& settings,
                                     DrawingSheetSettings* prepared,QString* error = nullptr) const;
    bool setDrawingSheetSettings(const QString& partId,const DrawingSheetSettings& settings,QString* error = nullptr);
    DrawingSheetSourceStatus drawingSheetSourceStatus(const QString& partId) const;
    QString drawingSourceDataToken(const QStringList& sourceBodyIds,QString* error = nullptr) const;
    bool refreshDrawingSheetSource(const QString& partId,QString* error = nullptr);
    // Explicitly revalidate one saved edge-linked drawing callout against the
    // CURRENT OCCT geometry before recording its new revision. A stale legacy
    // index-only annotation cannot be silently reattached: capture a fresh
    // witness via the drawing editor instead. One success = one undo record;
    // a failed preflight leaves the sheet and document byte-for-byte intact.
    bool reviewDrawingEdgeDimension(const QString& sheetOwnerId,const QString& annotationId,
                                    QString* error = nullptr);
    // Read-only multi-sheet preflight and atomic geometry-checked approval of
    // selected EDGE DIMENSIONS only. Every witness/projection must resolve
    // before any saved drawing changes. Does NOT acknowledge GD&T or the whole
    // sheet; callers must review those separately. One undo entry for the
    // entire selection; duplicate or broken IDs fail without a partial update.
    bool previewDrawingEdgeReviewBatch(const QMap<QString,QStringList>& selection,
                                       QString* report,QString* error=nullptr) const;
    bool reviewDrawingEdgeReviewBatch(const QMap<QString,QStringList>& selection,
                                      QString* report=nullptr,QString* error=nullptr);
    // Read-only and spreadsheet-safe cross-sheet recovery register. All saved
    // sheet/edge/GD&T links are reported, not silently approved.
    // Read-only consolidated engineering preflight. Executes actual OCCT
    // refold and drawing regeneration checks, validates piecewise-linear joint
    // schedules, and emits an actionable issue register. Never changes the
    // design, issues a drawing, or certifies collision-free motion. Individual
    // failed checks become BLOCK rows; invalid invocation preserves output.
    bool buildIntegratedEngineeringReadinessCsv(QByteArray* output,
        QString* error=nullptr) const;
    bool exportIntegratedEngineeringReadinessCsv(const QString& path,
        QString* error=nullptr) const;
    bool buildDrawingRecoveryCsv(QByteArray* output,QString* error=nullptr) const;
    bool exportDrawingRecoveryCsv(const QString& path,QString* error=nullptr) const;
    // Rebuild complete SVGs for 1-12 persisted sheets from CURRENT source
    // B-reps, view definitions, BOM and linked annotations. The operation is
    // read-only and atomic for both output maps; stale geometry-linked edge
    // dimensions and GD&T must be reviewed explicitly before publishing.
    bool buildSavedDrawingSvgBatch(const QStringList& sheetOwnerIds,
        QMap<QString,QByteArray>* svgByOwner,
        QMap<QString,DrawingSheetSettings>* preparedByOwner,
        QString* report=nullptr,QString* error=nullptr) const;
    // Publish all regenerated SVGs plus their source revisions in one NEW
    // directory. Never overwrites a released drawing or changes saved tokens.
    // Human visual approval and per-annotation sign-off remain outstanding.
    bool exportSavedDrawingSvgBatch(const QStringList& sheetOwnerIds,
        const QString& newDirectory,QString* report=nullptr,
        QString* error=nullptr) const;
    // Transactional shape-agnostic feature tree. Also works on existing/imported
    // surfaces; solid-only modifier types report a clear error on surfaces.
    // A direct external geometry edit makes the tree stale until explicitly rebased.
    bool setShapeFeatureTree(const QString& partId,const QVector<ShapeFeatureNode>& nodes,
                             QString* error = nullptr);
    // Independently editable multi-part manufacturing finish: all selected
    // bodies rebuild inside a disposable scene. On ANY kernel / dependent
    // failure nothing is committed, and success creates exactly one undo step.
    // The vector for each body is its entire proposed stable-ID feature tree.
    bool setShapeFeatureTreesBatch(const QMap<QString,QVector<ShapeFeatureNode>>& proposals,
                                   QString* error = nullptr);
    // Rebuilds all proposed roots and their linked descendants in a detached
    // scene, returning diagnostics but never creating an undo entry or changing
    // live geometry. Invalid intermediate geometry aborts the whole preview.
    bool previewShapeFeatureTreesBatch(const QMap<QString,QVector<ShapeFeatureNode>>& proposals,
                                       QString* report,QString* error = nullptr) const;
    bool rebaseShapeFeatureTree(const QString& partId,QString* error = nullptr);
    // Transactionally install/reorder a linear history of <=40 steps. Shape
    // feature operations and external sketch bosses/pockets may alternate in
    // any order. Cross-body source edits rebuild this history and descendants.
    // Unlike branched shape-feature graphs this path uses the previous result
    // for each regular feature node (no named branch inputs).
    bool setInterleavedFeatureHistory(const QString& partId,
                                      const QVector<InterleavedHistoryStep>& steps,
                                      QString* error=nullptr);
    // Read-only evaluation of a staged prefix for topology-aware feature editing.
    // Returns the frozen @base and each stable step ID -> rebuilt B-rep.
    bool previewInterleavedFeatureHistory(const QString& partId,
                         const QVector<InterleavedHistoryStep>& steps,
                         QMap<QString,TopoDS_Shape>* stageShapes,
                         QString* error=nullptr) const;
    // Unlike a prefix-only B-rep preview, this uses the full linked-body
    // regeneration planner in a disposable document, detecting invalid
    // downstream solids before the history editor stages an edit.
    bool previewInterleavedHistoryWithDependents(const QString& partId,
        const QVector<InterleavedHistoryStep>& steps,QString* report,
        QString* error=nullptr) const;
    bool interleavedFeatureHistoryCurrent(const QString& partId) const;
    bool shapeFeatureTreeCurrent(const QString& partId) const;
    bool addParametricRectangularExtrusion(double width,double height,double depth,QString* error = nullptr);
    bool addConstrainedPolygonExtrusion(const SketchModel& sketch,const QStringList& boundaryPointIds,
                                        double depth,QString* error = nullptr,const SketchPlane& plane = {});
    bool updateConstrainedPolygonExtrusion(const QString& partId,const SketchModel& sketch,
                                           const QStringList& boundaryPointIds,double depth,QString* error = nullptr,const SketchPlane& plane = {});
    bool addParametricRectangularExtrusionAt(double originX,double originY,double width,double height,double depth,QString* error = nullptr);
    bool addParametricThroughHole(const QString& partId,double diameter,double centerX,double centerY,QString* error = nullptr);
    bool addParametricBlindHole(const QString& partId,double diameter,double centerX,double centerY,
                                double holeDepthMm,QString* error = nullptr);
    bool addParametricLinearHolePattern(const QString& partId,int holeCount,double spacingMm,QString* error = nullptr);
    bool addParametricLinearHolePattern(const QString& partId,int holeCount,double spacingMm,
                                        const QString& axis,QString* error = nullptr);
    bool addParametricAllEdgeChamfer(const QString& partId,double distanceMm,QString* error = nullptr);
    bool addParametricAllEdgeFillet(const QString& partId,double radiusMm,QString* error = nullptr);
    bool setParameterLink(const QString& partId,const QString& target,const QString& source,
                          double scale,QString* error = nullptr);
    bool setParameterLink(const QString& partId,const QString& target,const QString& source,
                          double scale,double offset,QString* error = nullptr);
    bool removeParameterLink(const QString& partId,const QString& target,QString* error = nullptr);
    QMap<QString,ParameterLink> parameterLinks(const QString& partId) const;
    bool setNamedParameter(const QString& partId,const QString& parameterName,double value,QString* error = nullptr);
    bool setNamedParameters(const QString& partId,const QMap<QString,double>& values,QString* error = nullptr);
    QMap<QString,double> namedParameters(const QString& partId) const;
    QMap<QString,QString> namedParameterFormulas(const QString& partId) const;
    // Only stable shape-feature and interleaved-history step IDs are eligible.
    QStringList inspectionFeatureIds(const QString& partId) const;
    QVector<FeatureInspectionNote> featureInspectionNotes(const QString& partId) const;
    bool setFeatureInspectionNotes(const QString& partId,
                                   const QVector<FeatureInspectionNote>& notes,
                                   QString* error=nullptr);
    bool buildFeatureInspectionCsv(const QString& partId,QByteArray* csv,
                                   QString* error=nullptr) const;
    bool exportFeatureInspectionCsv(const QString& partId,const QString& fileName,
                                    QString* error=nullptr) const;
    QVector<SavedDesignConfiguration> designConfigurations(const QString& partId) const;
    bool setDesignConfigurations(const QString& partId,
                                 const QVector<SavedDesignConfiguration>& configurations,
                                 QString* error=nullptr);
    // Apply a persisted variant via the same atomic geometry-and-linked-body
    // rebuild used for manual parameter edits. One undo checkpoint only.
    bool applyDesignConfiguration(const QString& partId,const QString& name,
                                  QString* error=nullptr);
    // Includes a baseline CURRENT row. Any failing stored variant aborts the
    // entire report, preserving the caller's previous output unchanged.
    bool compareDesignConfigurations(const QString& partId,
                                     QVector<ConfigurationBomComparison>* result,
                                     QString* error=nullptr) const;
    bool buildConfigurationComparisonCsv(const QString& partId,QByteArray* csv,
                                         QString* error=nullptr) const;
    bool exportConfigurationComparisonCsv(const QString& partId,const QString& path,
                                          QString* error=nullptr) const;
    // Exact detached-document what-if rebuild: evaluates the proposed formula
    // graph and all linked descendants using the production commit path. No
    // geometry, revision, undo, or caller report is changed on failure.
    bool previewNamedParameterFormulas(const QString& partId,
                                       const QMap<QString,QString>& formulas,
                                       QString* report,QString* error=nullptr) const;
    bool setNamedParameterFormulas(const QString& partId,const QMap<QString,QString>& formulas,
                                   QString* error=nullptr);
    // Coordinated multi-body parameter changes: every source and its linked
    // descendants rebuild in a disposable document first. An accepted batch
    // creates one undo checkpoint, never one per part. Existing formulas for
    // explicitly edited dimensions follow the normal single-part semantics.
    bool previewNamedParameterBatch(const QMap<QString,QMap<QString,double>>& changes,
                                    QString* report,QString* error=nullptr) const;
    bool setNamedParameterBatch(const QMap<QString,QMap<QString,double>>& changes,
                                QString* error=nullptr);
    // One stage/one commit for MULTIPLE edited dimensions AND optional-feature
    // suppression across source bodies. Unlike sequential UI commands, a
    // temporarily invalid intermediate solid never blocks a valid final design.
    // Rebuilds each source once, then all linked descendants, with rollback on
    // any formula/link/topology error. At least two participating bodies.
    bool setCoordinatedParametricRevision(
        const QMap<QString,QMap<QString,double>>& dimensions,
        const QMap<QString,QStringList>& suppression,QString* error=nullptr);
    bool previewCoordinatedParametricRevision(
        const QMap<QString,QMap<QString,double>>& dimensions,
        const QMap<QString,QStringList>& suppression,
        QString* report,QString* error=nullptr) const;
    // Compare 1-8 independent multi-body design alternatives, including an
    // optional empty baseline. Invalid alternatives get a failure row; they
    // never partially modify the model or affect other candidates.
    bool evaluateParametricDesignAlternatives(
        const QVector<ParametricDesignAlternative>& alternatives,
        QVector<ParametricDesignAlternativeResult>* results,QString* error=nullptr) const;
    // Promote one evaluated dimension alternative using the SAME coordinated
    // source/descendant rebuild as interactive parameter editing. The caller
    // supplies the document geometry fingerprint captured at selection time;
    // a stale UI candidate cannot silently overwrite a newer design.
    QString designGeometryFingerprint() const;
    bool applyParametricDesignAlternative(const ParametricDesignAlternative& selected,
                    const QString& expectedGeometryFingerprint,QString* error=nullptr);
    // Change optional operations across multiple parametric source bodies as ONE
    // atomic feature-history transaction. Every linked descendant regenerates
    // after all sources have been rebuilt; an invalid feature or B-rep leaves
    // original geometry, history, and undo stack unchanged.
    bool setParametricFeaturesSuppressedBatch(const QMap<QString,QStringList>& byBody,
                                             QString* error=nullptr);
    bool previewParametricFeaturesSuppressedBatch(const QMap<QString,QStringList>& byBody,
                                                 QString* report,QString* error=nullptr) const;
    bool setParametricFeatureSuppressed(const QString& partId,const QString& featureId,bool suppressed,
                                        QString* error = nullptr);
    bool setParametricFeaturesSuppressed(const QString& partId,const QStringList& featureIds,
                                         QString* error = nullptr);
    QStringList suppressedParametricFeatures(const QString& partId) const;
    QStringList sketchConstraints(const QString& partId) const;
    QStringList featureHistory(const QString& partId) const;
    bool parametricDefinitionCurrent(const QString& partId) const;
    bool booleanOperation(BooleanOp op, const QString& firstId, const QString& secondId, QString* error = nullptr);
    // Creates a new non-destructive body, linked to two existing source bodies.
    // Feature-stack edits and sketch updates propagate transactionally.
    bool addLinkedBooleanBody(BooleanOp op,const QString& primaryId,const QString& toolId,
                              QString* error=nullptr);
    // Retarget an existing result without changing its persistent identity.
    // Rebuilds every affected linked descendant atomically; rejects cycles.
    bool updateLinkedBooleanBody(const QString& resultId,BooleanOp op,
                                 const QString& primaryId,const QString& toolId,
                                 QString* error=nullptr);
    // Atomic multi-result retarget. The final dependency graph is validated
    // before any geometry build; all changes become a single undo checkpoint.
    // Supports coordinated rewiring that an intermediate single edit would
    // temporarily make circular. Failed evaluation leaves the scene intact.
    bool updateLinkedBooleanBatch(const QVector<LinkedBooleanEdit>& edits,
                                  QString* error=nullptr);
    // Validates the proposed graph without building or committing geometry.
    // Includes edited results and all their downstream dependents, in DAG order.
    bool previewLinkedBooleanBatch(const QVector<LinkedBooleanEdit>& edits,
                                   QStringList* affectedIds,QString* error=nullptr) const;
    // Read-only impact preview; returns the selected source and all its
    // transitive linked-body dependents in rebuild order.
    bool linkedBodyImpact(const QString& sourceId,QStringList* bodyIds,
                          QString* error=nullptr,QStringList* explanationPaths=nullptr) const;
    bool refreshLinkedBooleanBodies(QString* error=nullptr);
    bool linkedBooleanCurrent(const QString& partId) const;
    // A live, single-source extrusion of another body's persisted sketch.
    // Source sketches must be current. All changes and Boolean dependents
    // update atomically, retaining source geometry and its stable ID.
    bool addLinkedSketchExtrusion(const QString& sourceBodyId,double depthMm,
                                 double normalOffsetMm,QString* error=nullptr);
    bool updateLinkedSketchExtrusion(const QString& derivedBodyId,double depthMm,
                                    double normalOffsetMm,QString* error=nullptr);
    // Reassign an existing sketch-derived body to another live constrained
    // sketch. Rebuilds its entire dependent DAG in one undoable transaction.
    bool retargetLinkedSketchExtrusion(const QString& derivedBodyId,
                                      const QString& sourceBodyId,double depthMm,
                                      double normalOffsetMm,QString* error=nullptr);
    // Create an associative subtractive pocket as TWO linked bodies: a hidden
    // sketch-derived tool and a visible linked Cut result. Both are committed
    // atomically, and both regenerate after changes to the source sketch.
    bool addLinkedSketchPocket(const QString& sketchSourceBodyId,
                               const QString& targetBodyId,double depthMm,
                               double normalOffsetMm,QString* error=nullptr);
    // Non-destructive additive counterpart to the associative pocket:
    // hidden source-linked tool plus a visible, one-solid Fuse result.
    // The complete two-body creation is a single atomic undo transaction.
    bool addLinkedSketchBoss(const QString& sketchSourceBodyId,
                             const QString& targetBodyId,double depthMm,
                             double normalOffsetMm,QString* error=nullptr);
    // Applies an external constrained sketch to an EXISTING target body rather
    // than creating a derived body. Up to eight ordered operations per target.
    // The original target solid and source link are persisted for regeneration.
    bool setInPlaceSketchFeature(const QString& targetBodyId,const QString& sketchSourceId,
                                 BooleanOp operation,double depthMm,double normalOffsetMm,
                                 QString* error=nullptr);
    bool removeInPlaceSketchFeature(const QString& targetBodyId,QString* error=nullptr);
    // Legacy second-stage API; stages 3-8 use the indexed stage methods. On failure, document and undo stack
    // remain unchanged; the base B-rep belongs to the original target.
    bool appendInPlaceSketchFeature(const QString& targetBodyId,const QString& sketchSourceId,
                                    BooleanOp operation,double depthMm,double offsetMm,
                                    QString* error=nullptr);
    bool updateInPlaceSketchFollowUp(const QString& targetBodyId,const QString& sketchSourceId,
                                     BooleanOp operation,double depthMm,double offsetMm,
                                     QString* error=nullptr);
    bool removeInPlaceSketchFollowUp(const QString& targetBodyId,QString* error=nullptr);
    bool appendInPlaceSketchStage(const QString& targetBodyId,const QString& sketchSourceId,
                                  BooleanOp operation,double depthMm,double offsetMm,
                                  QString* error=nullptr);
    // additionalIndex is zero-based: index 0 addresses stage 3.
    bool updateInPlaceSketchStage(const QString& targetBodyId,int additionalIndex,
                                  const QString& sketchSourceId,BooleanOp operation,
                                  double depthMm,double offsetMm,QString* error=nullptr);
    bool removeLastInPlaceSketchStage(const QString& targetBodyId,QString* error=nullptr);
    bool inPlaceSketchFeatureCurrent(const QString& targetBodyId) const;
    bool linkedSketchExtrusionCurrent(const QString& derivedBodyId) const;
    bool measureMinimumDistance(const QString& firstId,const QString& secondId,MinimumDistanceResult* result,
                                QString* error = nullptr) const;
    bool measureInterference(const QString& firstId,const QString& secondId,InterferenceResult* result,
                             QString* error = nullptr) const;
    bool diagnoseBody(const QString& id,BodyDiagnostics* result,QString* error = nullptr) const;
    bool measureMassFromDensity(const QString& id,double densityKgPerM3,
                                DensityMassProperties* result,QString* error = nullptr) const;
    // A centroidal inertia tensor in kg*mm^2 with principal moments, axes and radii.
    // This is a non-mutating solid measurement; edited geometry is always evaluated live.
    bool measureMassTensor(const QString& id,double densityKgPerM3,
                           tensor::Report* result,QString* error = nullptr) const;
    // Sum physical component inertias with the parallel-axis theorem.
    // Every specified body must have an assigned density; duplicates are rejected.
    bool measureAssemblyMassTensor(const QStringList& bodyIds,tensor::Report* result,
                                   QString* error = nullptr) const;
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
    // Select a planar face of ANY solid: use its inward normal and a world-space
    // point on its supporting plane. Preview remains atomic and token-guarded.
    bool prepareHoleOnPlanarFace(const FaceReference& face, double x,double y,double z,
                                 double diameter,double depth,bool throughAll,
                                 HoleCommand* command,QString* error=nullptr) const;
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
    // Atomic bulk rename; a swap is legal, but a collision with any final name is not.
    // Validate every body and name before ONE undo checkpoint. No geometry is rebuilt.
    bool renameBatch(const QMap<QString,QString>& desired, QString* error = nullptr);
    bool setVisibility(const QString& id, bool visible, QString* error = nullptr);
    // One undoable transaction for visibility changes across multiple bodies.
    bool setVisibilityBatch(const QMap<QString,bool>& desired, QString* error = nullptr);
    bool isolateBodies(const QStringList& selectedIds, QString* error = nullptr);
    bool showAllBodies(QString* error = nullptr);
    bool setColor(const QString& id, const QString& hexColor, QString* error = nullptr);
    bool setMaterial(const QString& id,const QString& materialName,double densityKgPerM3,
                     QString* error = nullptr);
    bool setMaterial(const QStringList& ids,const QString& materialName,double densityKgPerM3,
                     QString* error = nullptr);
    bool setMaterialEngineering(const QStringList& ids, const QString& provenance,
                                double youngsModulusGPa, QString* error = nullptr);
    // Derived, document-local list of assigned distinct material records; no global DB.
    QVector<MaterialLibraryEntry> documentMaterialLibrary() const;
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
    bool importOBJ(const QString& path,double weldToleranceMm = 0.01,QString* error = nullptr,
                   MeshImportSummary* repairSummary = nullptr,bool fillPlanarHoles = false,
                   bool importAsSurfaceBody = false);
    bool importSTL(const QString& path,double weldToleranceMm = 0.01,QString* error = nullptr,
                   MeshImportSummary* repairSummary = nullptr,bool fillPlanarHoles = false,
                   bool importAsSurfaceBody = false);
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
    // Atomically relocates every occurrence in an assembly group, including
    // nested descendants. Paths persist through existing scene JSON and STEP
    // hierarchy export; part geometry and occurrence transforms are untouched.
    // Rejects self-nesting, unintended group merges and invalid destinations.
    bool moveAssemblySubtree(const QString& oldPath,const QString& newPath,
                             QString* error = nullptr);
    // A read-only listing of the distinct occupied assembly groups, including
    // intermediate parents, sorted from parent to child.
    QStringList assemblyGroupPaths() const;
    QVector<BillOfMaterialsRow> billOfMaterials(QString* error = nullptr) const;
    QVector<BillOfMaterialsRow> billOfMaterials(const QStringList& bodyIds, QString* error = nullptr) const;
    QVector<PartsListRow> partsList(QString* error = nullptr) const;
    bool exportBillOfMaterialsCsv(const QString& path, QString* error = nullptr) const;
    bool exportPartsListCsv(const QString& path, QString* error = nullptr) const;
    StandardsCheckReport checkStandards(StandardsProfile profile) const;

private:
    bool validateJointMotionKeyframesBatch(
        const QMap<QString,QVector<JointMotionKeyframe>>& tracks,
        bool* changed,QString* report,QString* error) const;

    // Single staged commit for direct machining edits and repeated patterns.
    // Protects source-linked descendants and the undo journal as one unit.
    bool commitMachiningShape(const QString& bodyId,const QString& expectedRevision,
                              const TopoDS_Shape& result,QString* error);
    bool stageShapeFeatureTreesBatch(const QMap<QString,QVector<ShapeFeatureNode>>& proposals,
                                     SceneDocument* staged,bool* changed,
                                     QStringList* changedBodyIds,QString* error) const;
    bool stageDrawingEdgeReviewBatch(const QMap<QString,QStringList>& selection,
            QMap<QString,DrawingSheetSettings>* updated,QString* report,QString* error) const;
    QVector<BillOfMaterialsRow> buildBillOfMaterials(const QStringList& bodyIds, QString* error) const;
    struct Snapshot {
        QVector<Part> parts;
        DrawingReadinessPolicy drawingReadinessPolicy;
        quint64 revision;
        QString liveRigidMateFixed;
        QVector<AssemblyNetworkMate> liveRigidMates;
        QString liveArticulatedFixed;
        QVector<ArticulatedNetworkJoint> liveArticulatedJoints;
        QMap<QString,mates::Transform> liveArticulatedReferencePoses;
    };
    QVector<Part> parts_;
    QString liveRigidMateFixed_;
    QVector<AssemblyNetworkMate> liveRigidMates_;
    QString liveArticulatedFixed_;
    QVector<ArticulatedNetworkJoint> liveArticulatedJoints_;
    QMap<QString,mates::Transform> liveArticulatedReferencePoses_;
    DrawingReadinessPolicy drawingReadinessPolicy_;
    QVector<Snapshot> undo_;
    QVector<Snapshot> redo_;
    QString path_;
    quint64 revision_ = 0;
    quint64 savedRevision_ = 0;
    quint64 nextRevision_ = 1;
    void checkpoint();
    // Read-only staging pass shared by refresh and joint edits. No checkpoint,
    // cache mutation, or definition writes before every pose is validated.
    bool stageLiveArticulatedRegeneration(const QVector<ArticulatedNetworkJoint>& joints,
                                           QVector<Part>* staged,bool* changed,
                                           QString* error) const;
    bool stageArticulatedDefinitionBatch(
        const QMap<int,ArticulatedNetworkJoint>& edits,
        QVector<ArticulatedNetworkJoint>* definitions,
        QVector<Part>* staged,bool* geometryChanged,bool* definitionsChanged,
        QString* report,QString* error,double clearanceMm,
        bool rejectUnsafeNominal) const;
    // Recompute all downstream linked Boolean bodies in insertion order. A
    // failed descendant stops the entire transaction before a checkpoint.
    bool propagateLinkedBodies(QVector<Part>* staged,QString* error) const;
    bool setNamedParametersWithLinks(const QString& partId,const QMap<QString,double>& values,
                                     const QMap<QString,ParameterLink>& links,bool allowEmpty,
                                     QString* error,const QMap<QString,QString>* replacementFormulas=nullptr,
                                     const QString* selectedConfiguration=nullptr);
    QString uniqueName(const QString& base) const;
    static bool validShape(const TopoDS_Shape& shape, QString* error);
    bool addSweepProfile(bool circular,bool elliptical,double width,double height,double pathLength,double lateralOffset,
                         double verticalRise,QString* error);
    static void setError(QString* error, const QString& text);
};
} // namespace cad
