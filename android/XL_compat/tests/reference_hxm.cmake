foreach(
    pair IN
    ITEMS
        "include/sphere.h|59b88a1d94c792508de1910a344c07d6ff2469c89f46821553c7b462f2013252"
        "render/sphere.cpp|14ea746fbecd4b98f7985fdb11a87443a882108773634202bf9271c7a19a8d63"
        "include/objeffects.h|1d0ae9e924d18bfc95faa99b759300db01224d95946a0a023d2ae4afac9d461b"
        "render/rendershields.cpp|80289700274a840bc6de57652b0d42e93f0d3e845802c776d310352500d5a003"
        "objects/objectio.cpp|528f64e09469b5a452e9c6cf0ae8e9d5dd3eaffeaf5478378669f0783deaf68f"
        "include/carray.h|10ea5f725a079e9be98f7871ffa6544d1de5c427db472b214b3f91188719019c"
        "gameio/loadgame.cpp|5d7ff36d6121411e0250be94353bbe3268bdd02b9bd35c6161951d879fcecc55"
        "gameio/savegame.cpp|4784770058b2d4880c33a81eb1a7611738959113a80bce48de248eee7a54a795"
        "include/network.h|db4afcb3fa29cf859a97a4c32816abefd645a12b65ce6786151f1591a4e9cad7"
        "gameio/playerprofile.cpp|43177ca6d27def5d85f2c075fb7a9e703eba3b486f26b4e7fae4d75c6db9a478"
        "3d/hitbox.cpp|2062a3230e07a0a00ff880621d22e66e322330f7c98c6f6e5d28896ed8a25eed"
        "models/polymodel.cpp|5e0b43cc1b12b5cc8531bf6f3fd81ecc10703b0ea5189ad4c7ce70dec3e24f17"
        "gameio/loadmodeldata.cpp|4b191d4abf1a3a68348b468bd6db77da805c2334ba9d49d623697c294c9bfb8c"
        "include/vecmat.h|83d7f73f89a2f588a0b56a08fb9e293b747cc65a5ed22bad6771d64319bdca0f"
        "include/maths.h|43adf417dd1c282a39c8fed02bdad9d05a7f3ad5850a39a87897fd533eb857cd"
        "maths/vecmat.cpp|4115afe844ad77a81e9f0de57ae1bbf577b06d5f49aed8b02d19002ff2021335"
        "physics/hitbox_collision.cpp|506e0d824dc2c1bd87e6cb69d84a021ab1a54773bc42fb09b9e3dda5bb1d84f7"
        "physics/sphere_collision.cpp|499873929ce6606f00b504e59624595e6bee57f9f4e225f53c85ebd0c1a80b60"
        "physics/physics.cpp|2ca8d5c939f31da9d4686637ed3e4305bf89e208a575f2f7f6ea2b4d2e6fab76"
        "objects/criticalhit.cpp|eafa19e6b881a5370e5ba48ead2c52dcd8ac02a664c42dc9ed58161a371c90e2"
        "include/descent.h|f8c34f569c51541dc4d1e69ae3ac04972401b4c6cad99ac450bbc57a245f83e4"
        "3d/pof2model.cpp|34f9c3a91872bd3fddcc271571291948c71775e501567b9e6f1271678055a26f"
        "3d/buildmodel.cpp|5bf1b3ee660b45e50da2d4516d59665f654f8964ec2738cb7ccd92738e36d7d9"
        "include/collision_math.h|06512749bd4db99f49d55c9ff1155d524b629b3fb5b7fecf6e03c7ba38b2a3a1")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "HXM reference changed: ${path}")
    endif()
endforeach()
set(extracted "")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/maths.h" hitbox_math)
foreach(
    name IN
    ITEMS XRound
          FixDiv
          mul64
          FixMul
          X2F
          F2X
          I2X)
    string(REGEX MATCH "#[ \t]*define ${name}\\([^\n]+" macro "${hitbox_math}")
    if(NOT macro)
        message(FATAL_ERROR "Missing hitbox math macro ${name}")
    endif()
    string(APPEND extracted "${macro}\n")
endforeach()
foreach(signature IN ITEMS "fix CFixVector::Mag (void) const" "void CFixVector::Check ("
                           "fix FixQuadAdjust (" "const CFixVector CFixVector::Cross (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp" "${signature}")
endforeach()
foreach(
    signature IN
    ITEMS "inline const CFixVector CFixVector::operator- (const CFixVector&"
          "inline const fix CFixVector::Normalize (" "inline const CFixVector CFixVector::Perp ("
          "inline const CFixVector CFixVector::Normal (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" "${signature}")
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/3d/hitbox.cpp" hitbox_source)
string(FIND "${hitbox_source}" "CFixVector hitBoxOffsets [8]" tables_start)
string(FIND "${hitbox_source}" "void ComputeHitbox (" tables_end)
if(tables_start LESS 0 OR tables_end LESS tables_start)
    message(FATAL_ERROR "Missing source hitbox topology tables")
endif()
math(EXPR tables_length "${tables_end} - ${tables_start}")
string(SUBSTRING "${hitbox_source}" ${tables_start} ${tables_length} tables)
string(APPEND extracted "${tables}\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/3d/hitbox.cpp" "void ComputeHitbox (")
foreach(
    signature IN
    ITEMS "inline const fix CFixVector::Dot (const CFixVector&"
          "inline const CFixVector& CFixVector::operator+= (const CFixVector&"
          "inline CFixVector CFixMatrix::operator* (const CFixVector&")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/3d/hitbox.cpp" "tHitbox* TransformHitboxes (")
foreach(
    signature IN
    ITEMS "inline const fix CFixVector::Dist ("
          "inline CFloatVector& CFloatVector::Assign (const CFixVector&"
          "inline CFixVector& CFixVector::Assign (const CFloatVector&"
          "inline const float CFloatVector::Dot (const CFloatVector&"
          "inline const CFloatVector& CFloatVector::operator*= (const float")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" "${signature}")
endforeach()
foreach(
    signature IN
    ITEMS "inline const float CFloatVector::SqrMag ("
          "inline const float CFloatVector::Mag ("
          "inline const float CFloatVector::Normalize ("
          "inline const float CFloatVector::Dist ("
          "inline void CFloatVector::Set (const float f0"
          "inline const CFloatVector& CFloatVector::operator/= (const float"
          "inline const CFloatVector& CFloatVector::operator+= (const CFloatVector&"
          "inline const CFloatVector CFloatVector::operator+ (const CFloatVector&"
          "inline const CFloatVector CFloatVector::operator- (const CFloatVector&"
          "inline const CFloatVector CFloatVector::operator* (const float")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" "${signature}")
endforeach()
string(APPEND extracted "\n#define FLOAT_COLLISION_MATH 1\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/sphere_collision.cpp"
                      "int32_t CheckVectorSphereCollisionf (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/sphere_collision.cpp"
                      "int32_t CheckVectorSphereCollision (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp"
                      "const int32_t FindPointLineIntersection (CFixVector&")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/physics/sphere_collision.cpp" sphere_source)
string(REGEX MATCH "int32_t ijTable \\[3\\]\\[2\\] = \\{[^}]+\\}[^;]+;" axes "${sphere_source}")
if(NOT axes)
    message(FATAL_ERROR "Missing hitbox projection axes")
endif()
string(APPEND extracted "${axes}\n")
foreach(
    signature IN
    ITEMS "static bool PointIsInQuad (" "static fix DistToQuad ("
          "static int32_t FindLinePlaneIntersection (" "static int32_t FindLineQuadIntersection (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/hitbox_collision.cpp"
                          "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/3d/hitbox.cpp" "int32_t GetPolyModelMinMax (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/models/polymodel.cpp" "fix CPolyModel::Size (")
extract_face_function(
    "${XL_REFERENCE_GEOMETRY_ROOT}/include/collision_math.h"
    "static inline fix RegisterHit (CFixVector *vBestHit, CFixVector *vBestNormal,")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/hitbox_collision.cpp"
                      "static int32_t FindLineHitboxIntersection (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/hitbox_collision.cpp"
                      "fix CheckVectorHitboxCollision (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h"
                      "inline const CFixVector& CFixVector::operator/= (const fix")
foreach(
    signature IN
    ITEMS "static int32_t FindQuadQuadIntersectionSub ("
          "static int32_t FindQuadQuadIntersection (" "static int32_t FindHitboxIntersection ("
          "fix CheckHitboxCollision (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/hitbox_collision.cpp"
                          "${signature}")
endforeach()
foreach(
    signature IN
    ITEMS "inline bool CFixVector::IsZero ("
          "inline const CFixVector& CFixVector::operator*= (const fix"
          "inline const CFixVector CFixVector::operator* (const fix")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/physics.cpp"
                      "void CObject::UnstickFromObject (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp"
                      "const fix VmLinePointDist (const CFixVector&")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/descent.h"
                      "static inline CFixVector *PolyObjPos (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/sphere_collision.cpp"
                      "fix CheckVectorObjectCollision (")
string(REPLACE "fix CheckVectorObjectCollision (" "fix ReferenceVectorObjectCollision (" extracted
               "${extracted}")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h"
                      "inline const CFixVector CFixVector::Avg (const CFixVector&")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/criticalhit.cpp"
                      "CFixVector CObject::RegisterHit (")
string(REPLACE "CFixVector CObject::RegisterHit (" "CFixVector CObject::RegisterHitSource ("
               extracted "${extracted}")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/3d/pof2model.cpp"
                      "int32_t CModel::CountPOFModelItems (")
string(REPLACE "int32_t CModel::CountPOFModelItems (" "int32_t RenderCount::CountPOFModelItems ("
               extracted "${extracted}")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/objects/objectio.cpp" object_io)
foreach(mode IN ITEMS Save Load)
    if(mode STREQUAL "Save")
        set(first "cf.WriteFix (m_damage.nHits [0]);")
        set(last "cf.WriteFix (gameStates.app.nSDLTicks [0] - m_damage.tRepaired);")
    else()
        set(first "m_damage.nHits [0] = cf.ReadFix ();")
        set(last "m_damage.tRepaired -= cf.ReadFix ();")
    endif()
    string(FIND "${object_io}" "${first}" begin)
    string(FIND "${object_io}" "${last}" end)
    if(begin LESS 0 OR end LESS begin)
        message(FATAL_ERROR "Missing source hit-state ${mode} fields")
    endif()
    string(LENGTH "${last}" last_length)
    math(EXPR length "${end} + ${last_length} - ${begin}")
    string(SUBSTRING "${object_io}" ${begin} ${length} body)
    string(APPEND extracted "\nvoid CObject::${mode}HitStateReference(CFile& cf) {\n${body}\n}\n")
endforeach()
set(hit_state_prefix "${extracted}")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/carray.h" array_source)
string(FIND "${array_source}" "void Clear (uint8_t filler" clear_begin)
if(clear_begin LESS 0)
    message(FATAL_ERROR "Missing source array Clear method")
endif()
string(SUBSTRING "${array_source}" ${clear_begin} -1 clear_tail)
string(FIND "${clear_tail}" "// ----------------------------------------" clear_end)
if(clear_end LESS 0)
    message(FATAL_ERROR "Missing source array Clear end marker")
endif()
string(SUBSTRING "${clear_tail}" 0 ${clear_end} extracted)
set(extracted
    "${hit_state_prefix}\n#define DBG_ARRAYS 0\ntemplate<typename _T> struct HitStateArray {\nstruct { _T* buffer; uint32_t length; } m_data;\n${extracted}\n};\n"
)
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/render/sphere.cpp"
                      "int32_t NeedEffect (CObject*")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h"
                      "inline const CFloatVector& CFloatVector::operator-= (const CFloatVector&")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h"
                      "inline const bool CFloatVector::operator== (const CFloatVector&")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h"
                      "inline CFixMatrix& CFixMatrix::Transpose (CFixMatrix& m)")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp"
                      "const fix CFixMatrix::Det (void)")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp"
                      "const CFixMatrix CFixMatrix::Inverse (void)")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/render/sphere.cpp" sphere_render)
string(FIND "${sphere_render}" "int32_t SetupHitEffectShader (" shader_begin)
string(SUBSTRING "${sphere_render}" ${shader_begin} -1 shader_tail)
string(FIND "${shader_tail}" "for (int32_t i = 0; i < 3; i++)" loop_begin)
string(FIND "${shader_tail}" "\nif (!ogl.UseTransform ()) {\n\ttransformation.End" loop_end)
if(shader_begin LESS 0 OR loop_begin LESS 0 OR loop_end LESS loop_begin)
    message(FATAL_ERROR "Missing source hit shader scalar loop")
endif()
math(EXPR loop_length "${loop_end} - ${loop_begin}")
string(SUBSTRING "${shader_tail}" ${loop_begin} ${loop_length} shader_loop)
string(FIND "${shader_tail}" "m = pPos->mOrient;" matrix_begin)
string(FIND "${shader_tail}" "m = m.Inverse ();" matrix_end)
if(matrix_begin LESS 0 OR matrix_end LESS matrix_begin)
    message(FATAL_ERROR "Missing source hit shader matrix setup")
endif()
string(LENGTH "m = m.Inverse ();" matrix_last_length)
math(EXPR matrix_length "${matrix_end} + ${matrix_last_length} - ${matrix_begin}")
string(SUBSTRING "${shader_tail}" ${matrix_begin} ${matrix_length} shader_matrix)
string(
    APPEND
    extracted
    "\nSourceHitEnvelope ReferenceHitEnvelope(CObject* pObj) {\nCObjHitInfo hitInfo = pObj->HitInfo();\nfloat fSize = 1.0f + 2.0f / X2F(pObj->Size());\nif (!ogl.UseTransform()) fSize *= X2F(pObj->Size());\nfloat fScale[3]; CFloatVector vHitf[3]; int32_t nHits = 0;\nCFixMatrix m{}; auto* pPos = &pObj->info.position; if (ogl.UseTransform()) { ${shader_matrix} }\n${shader_loop}\nSourceHitEnvelope result{nHits, {fScale[0], fScale[1], fScale[2]}, {}};\nfor (unsigned i=0;i<3;++i) result.vectors[i] = {vHitf[i].v.coord.x,vHitf[i].v.coord.y,vHitf[i].v.coord.z,vHitf[i].v.coord.w};\nreturn result;\n}\n"
)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_hxm_geometry.h"
     CONTENT "${extracted}")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/sphere.h" sphere_header)
string(FIND "${sphere_header}" "class CSphereVertex {" vertex_begin)
string(FIND "${sphere_header}" "class CSphereFace {" vertex_end)
if(vertex_begin LESS 0 OR vertex_end LESS vertex_begin)
    message(FATAL_ERROR "Missing original sphere vertex arithmetic")
endif()
math(EXPR vertex_length "${vertex_end} - ${vertex_begin}")
string(SUBSTRING "${sphere_header}" ${vertex_begin} ${vertex_length} sphere_vertex)
set(extracted
    "union tTexCoord2f { float a[2]; struct {float u,v;} v; };\n${sphere_vertex}\nfloat CSphereVertex::m_fNormRadScale=1.0f;\n"
)
string(
    APPEND
    extracted
    [=[
struct CSphereTriangle { CSphereVertex m_v[3]; };
struct CSphereQuad {
    CSphereVertex m_v[4], m_vCenter;
    CSphereVertex* ComputeCenter(int32_t bNormalize=1);
    CSphereQuad* Split(CSphereQuad*);
    CSphereTriangle* Split(CSphereTriangle*);
};
template<typename T> struct SphereBuffer {
    std::vector<T> values;
    int Create(int count,const char*) { values.resize(count); return 1; }
    T* Buffer() { return values.data(); }
    T& operator[](int index) { return values.at(index); }
    void Destroy() { values.clear(); }
};
#define SPHERE_MIN_QUALITY 2
template<typename T> static T Max(T a,T b) { return std::max(a,b); }
static void PrintLog(int) { throw std::runtime_error("Unexpected source sphere allocation failure"); }
struct CTesselatedSphere {
    int m_nQuality=0;
    int32_t Quality();
    void SetQuality(int quality) { m_nQuality=quality; }
};
struct CQuadSphere : CTesselatedSphere {
    SphereBuffer<CSphereQuad> m_quads[2];
    SphereBuffer<CSphereTriangle> m_faces;
    SphereBuffer<CSphereVertex> m_worldVerts,m_viewVerts;
    int m_nFaces=0,m_nVertices=0,m_nFaceBuffer=0;
    void SetupFaces();
    int32_t Tesselate(CSphereQuad*,CSphereQuad*,int32_t);
    int32_t Tesselate(CSphereTriangle*,CSphereQuad*,int32_t);
    int32_t CreateFaces();
    int32_t CreateBuffers();
    int32_t Create();
    // Outline edges are not part of the triangle vertex/UV output qualified here
    int CreateEdgeList() { return 0; }
};
]=])
foreach(
    signature IN
    ITEMS "void CSphereVertex::Normalize (void)"
          "CSphereVertex *CSphereQuad::ComputeCenter ("
          "CSphereQuad *CSphereQuad::Split ("
          "CSphereTriangle *CSphereQuad::Split ("
          "int32_t CTesselatedSphere::Quality (void)"
          "int32_t CQuadSphere::Tesselate (CSphereQuad"
          "int32_t CQuadSphere::Tesselate (CSphereTriangle"
          "void CQuadSphere::SetupFaces (void)"
          "int32_t CQuadSphere::CreateFaces (void)"
          "int32_t CQuadSphere::CreateBuffers (void)"
          "int32_t CQuadSphere::Create (void)")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/render/sphere.cpp" "${signature}")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_shield_sphere.h"
     CONTENT "${extracted}")
add_executable(xl_hxm_reference tests/reference_hxm.cpp)
set(extracted "")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/render/rendershields.cpp"
                      "void RenderPlayerShield (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_player_shield.h"
     CONTENT "${extracted}")
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/include/timeout.h" timeout_hash)
if(NOT timeout_hash STREQUAL "426a54d8313ba483ca32e360c7d5dbaa2577f1d5438b8c7df7f68bae9d525ee5")
    message(FATAL_ERROR "Pinned pulse timeout changed")
endif()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/timeout.h" timeout_source)
string(FIND "${timeout_source}" "class CTimeout {" begin)
string(FIND "${timeout_source}" "\n#endif /* TIMEOUT_H_ */" end)
math(EXPR length "${end} - ${begin}")
string(SUBSTRING "${timeout_source}" ${begin} ${length} extracted)
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/descent.h" descent_source)
string(FIND "${descent_source}" "class CPulseData {" begin)
string(SUBSTRING "${descent_source}" ${begin} -1 pulse_tail)
string(FIND "${pulse_tail}"
            "\n//------------------------------------------------------------------------------"
            end)
string(SUBSTRING "${pulse_tail}" 0 ${end} pulse_class)
string(APPEND extracted "\n${pulse_class}\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/render/sphere.cpp" "void CPulseData::Setup (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/render/sphere.cpp" "void CPulseData::Update (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_shield_pulse.h"
     CONTENT "${extracted}")
foreach(
    pair IN
    ITEMS
        "include/addon_bitmaps.h|9310ae945a30718aca25bc2a20622eaaf3dbf3e05d592cb8b0ee65d0f5269611"
        "gameio/addon_bitmaps.cpp|fba2bb3befbd5264788210b7c346b1203404c42a25790f0aff929bd6d67edc94"
        "include/bitmap.h|9044979682a8e219fa0526cc092150e562d04d2d772b23ce1e63d164d58c7024")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Pinned addon animation changed: ${path}")
    endif()
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/bitmap.h" bitmap_source)
string(FIND "${bitmap_source}" "inline CBitmap *NextFrame (void) {" begin)
string(FIND "${bitmap_source}" "inline CBitmap *CurFrame (int32_t" end)
math(EXPR length "${end} - ${begin}")
string(SUBSTRING "${bitmap_source}" ${begin} ${length} extracted)
string(REPLACE "*NextFrame (" "*CBitmap::AdvanceFrame (" extracted "${extracted}")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/addon_bitmaps.h" addon_source)
string(FIND "${addon_source}" "class CAddonBitmap : public CTGA {" begin)
string(SUBSTRING "${addon_source}" ${begin} -1 addon_tail)
string(FIND "${addon_tail}"
            "\n//------------------------------------------------------------------------------"
            end)
string(SUBSTRING "${addon_tail}" 0 ${end} addon_class)
string(APPEND extracted "\n${addon_class}\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/addon_bitmaps.cpp"
                      "CAddonBitmap::CAddonBitmap (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_addon_animation.h"
     CONTENT "${extracted}")
set(extracted "")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/3d/buildmodel.cpp"
                      "void CSubModel::InitMinMax (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/3d/buildmodel.cpp"
                      "void CSubModel::SetMinMax (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/3d/pof2model.cpp"
                      "CFace* CModel::AddPOFFace (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/3d/pof2model.cpp"
                      "int32_t CModel::GetPOFModelItems (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/3d/hitbox.cpp" "void ComputeHitbox (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/3d/buildmodel.cpp" "int32_t CModel::Shift (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/3d/buildmodel.cpp" "void CSubModel::Size (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/3d/buildmodel.cpp" "fix CModel::Size (")
foreach(
    signature IN
    ITEMS "inline const float CFloatVector3::SqrMag ("
          "inline const float CFloatVector3::Mag ("
          "inline const float CFloatVector3::Dist ("
          "inline CFloatVector3& CFloatVector3::Assign (const CFixVector&"
          "inline const CFloatVector3& CFloatVector3::operator+= ("
          "inline const CFloatVector3 CFloatVector3::operator+ ("
          "inline const CFloatVector3 CFloatVector3::operator- (const CFloatVector3&"
          "inline const CFloatVector3 CFloatVector3::operator* (float")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/3d/buildmodel.cpp"
                      "int32_t CModel::CmpVerts (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/3d/buildmodel.cpp"
                      "uint16_t CModel::FilterVertices (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/3d/buildmodel.cpp" "fix CModel::Radius (")
string(REPLACE "RenderModel::CModel::CmpVerts" "CModel::CmpVerts" extracted "${extracted}")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_pof_render.h" CONTENT "${extracted}")
target_include_directories(xl_hxm_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(xl_hxm_reference PRIVATE xl_inventory)
set(extracted "")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/network.h"
                      "static inline int32_t EGIFlag (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/network.h"
                      "static inline int32_t CollisionModel (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/hitbox_collision.cpp"
                      "int32_t UseHitbox (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/sphere_collision.cpp"
                      "int32_t UseSphere (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_collision_profile.h"
     CONTENT "${extracted}")
add_test(NAME xl_hxm_geometry_reference COMMAND xl_hxm_reference)
include(tests/reference_robot_animation.cmake)
