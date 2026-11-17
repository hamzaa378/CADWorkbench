"""Static cross-file preflight only; this is not a substitute for an MSVC build."""
from pathlib import Path
import re, unittest

ROOT=Path(__file__).resolve().parents[1]
def source(path):return (ROOT/path).read_text(encoding='utf-8')

class V158Preflight(unittest.TestCase):
    def test_unique_sketch_plane_types(self):
        headers='\n'.join(p.read_text(encoding='utf-8') for p in (ROOT/'src').rglob('*.h'))
        self.assertEqual(len(re.findall(r'\bstruct\s+SketchPlane\s*\{',headers)),1)
        self.assertEqual(len(re.findall(r'\benum class\s+ExtrusionSketchPlane\s*\{',headers)),1)
        self.assertNotRegex(headers,r'enum class\s+SketchPlane')
        for file in ('src/core/SceneDocument.cpp','src/ui/MainWindow.cpp'):
            s=source(file)
            self.assertNotIn('static_cast<cad::SketchPlane>(',s)
        self.assertIn('ExtrusionSketchPlane sketchPlane',headers)
        for path in (ROOT/'src').rglob('*.cpp'):
            code=path.read_text(encoding='utf-8')
            self.assertNotIn('SketchPlane::XY',code.replace('ExtrusionSketchPlane::XY',''),str(path))
        old_test=source('tests/cadcore_tests.cpp')
        self.assertNotIn('cad::SketchPlane::',old_test)
        self.assertNotIn('static_cast<cad::SketchPlane>',old_test)
        # Regression: these legacy extrusion fixtures must use the enum, not
        # the independent datum-plane struct introduced for 3D sketch profiles.
        self.assertRegex(old_test, r'struct PlaneCase\s*\{\s*cad::ExtrusionSketchPlane\s+plane;')
        self.assertRegex(old_test, r'struct ThroughAllCase\s*\{\s*cad::ExtrusionSketchPlane\s+plane;')
        self.assertNotRegex(old_test, r'struct (?:PlaneCase|ThroughAllCase)\s*\{\s*cad::SketchPlane')
        self.assertIn('command.sketchPlane=test.plane',old_test)

    def test_core_ui_operations_match(self):
        core=source('src/core/ShapeFeatureTree.cpp')
        ui=source('src/ui/MainWindow.cpp')
        for operation in ('Mirror XY','Mirror XZ','Mirror YZ','Linear pattern X',
                          'Linear pattern Y','Linear pattern Z','Circular pattern Z',
                          'Cylinder cut X','Cylinder cut Y','Cylinder cut Z',
                          'Face rectangle pocket','Face rectangle boss','Open-face shell',
                          'Draft face Z','Face normal pocket','Face normal boss'):
            self.assertIn('"'+operation+'"',core,operation)
            self.assertIn('"'+operation+'"',ui,operation)
        for parameter in ('count','spacing','sweep_degrees','face_index',
                          'thickness','angle_degrees','width','height','depth'):
            self.assertIn('"'+parameter+'"',core)

    def test_interactive_sketch_tools(self):
        header=source('src/ui/SketchCanvasDialog.h')
        code=source('src/ui/SketchCanvasDialog.cpp')
        for method in ('offsetStraightOutline','alignEdgeUnderCursor','removeCurveUnderCursor'):
            self.assertIn(method,header)
            self.assertIn('SketchCanvasWidget::'+method,code)
        for operation in ('Tool::Rectangle','Tool::Slot','"Horizontal edge"','"Vertical edge"',
                          '"Straighten curved edge"','"Offset convex outline..."'):
            self.assertIn(operation,code)
        self.assertIn('regenerateBoundarySegments();closed_=true;',code)

    def test_user_interface_and_error_recovery(self):
        code=source('src/ui/MainWindow.cpp')
        self.assertIn('ModelBrowserFilter',code)
        self.assertIn('Validate staged geometry (dry run)',code)
        self.assertIn('rebuildShapeFeatureTree(source,current->isSurfaceBody,nodes',code)
        self.assertIn('validateShapeFeatureNodes(nodes,&graphError)',code)
        self.assertIn('STALE: direct geometry edit detected',code)
        self.assertIn('PropertiesScrollArea',code)
        self.assertIn('shapeFeatureTreeCurrent(part.id)',code)
        self.assertIn('shapeFeatureCachedOutputs.size()==part.shapeFeatureNodes.size()',source('src/core/SceneDocument.cpp'))

    def test_build_and_regression_registration(self):
        cmake=source('CMakeLists.txt')
        version=re.search(r'project\(CADWorkbench VERSION (\d+)\.(\d+)\.(\d+)',cmake)
        self.assertIsNotNone(version)
        self.assertGreaterEqual(tuple(int(x) for x in version.groups()),(1,58,0))
        self.assertIn('cadcore_feature_upgrade_v158',cmake)
        self.assertTrue((ROOT/'tests/feature_upgrade_v158_tests.cpp').is_file())
        self.assertIn('QTemporaryDir',source('tests/feature_upgrade_v158_tests.cpp'))
        self.assertIn('test_unique_sketch_plane_types',source('tests/v158_source_preflight.py'))

    def test_no_accidental_multiline_string_literals(self):
        # Basic C++ tokenizer sufficient to catch accidental newline inside
        # ordinary strings, while allowing raw strings like R"CAD(...)CAD".
        for filename in ('src/ui/MainWindow.cpp','src/ui/SketchCanvasDialog.cpp',
                         'src/core/ShapeFeatureTree.cpp','src/core/SceneDocument.cpp',
                         'tests/feature_upgrade_v158_tests.cpp'):
            code=source(filename)
            state='code';i=0;line=1;rawend=''
            while i<len(code):
                ch=code[i]
                if ch=='\n':
                    if state in ('string','char'):
                        self.fail(f'{filename}: accidental line break in {state} at line {line}')
                    line+=1
                    if state=='linecomment':state='code'
                    i+=1;continue
                if state=='code':
                    if code.startswith('//',i):state='linecomment';i+=2;continue
                    if code.startswith('/*',i):state='blockcomment';i+=2;continue
                    m=re.compile(r'R"([^()\\\s]{0,16})\(').match(code,i)
                    if m:
                        rawend=')'+m.group(1)+'"';state='raw';i=m.end();continue
                    if ch=='"':state='string'
                    elif ch=="'":state='char'
                elif state=='raw':
                    if code.startswith(rawend,i):state='code';i+=len(rawend);continue
                elif state=='blockcomment':
                    if code.startswith('*/',i):state='code';i+=2;continue
                elif state in ('string','char'):
                    if ch=='\\':i+=2;continue
                    if ch==('"' if state=='string' else "'"):state='code'
                i+=1
            self.assertIn(state,('code','linecomment'),f'{filename}: unterminated {state}')

if __name__=='__main__':unittest.main(verbosity=2)
