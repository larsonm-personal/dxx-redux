foreach(
    pair IN
    ITEMS
        "objects/effectobject.cpp|0a2fcf48756916f8bdb65f1ee01cc06fd9d108fb02d0a81d3b0d3ab1b3b0e048"
        "include/object.h|ed546edd2bbda56a84e9fef8543706707d47a08b48e36616852346b5a2f96127"
        "include/particles.h|307e9edca466924df459b6ba5e6f0ec9d405967b38ed07021a9e561dfc29f70d"
        "include/objsmoke.h|c803d2adb2e3470d593a2095f34935e4033f13f433e5b4639bfda7193f64573a"
        "effects/objsmoke.cpp|ce39fc41866b703f0978af4ec0af6b4b98e30071b6c8015eaa60b719f8d32b80"
        "include/maths.h|43adf417dd1c282a39c8fed02bdad9d05a7f3ad5850a39a87897fd533eb857cd"
        "include/segment.h|4f43198ba394971128c81373940b5a58283cbbbc2851ca6a6e4187e78382a667"
        "include/fix.h|02c3d92d65d137e3d746cbe292fdaf32d4b7c997c9ac7a87c4375b509a5dca22"
        "include/vecmat.h|83d7f73f89a2f588a0b56a08fb9e293b747cc65a5ed22bad6771d64319bdca0f")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Particle source changed: ${path}")
    endif()
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/object.h" header)
set(extracted "// Actual pinned particle constants and authored parameter conversion\n")
foreach(
    name IN
    ITEMS OBJ_EFFECT
          PARTICLE_ID
          RT_PARTICLES
          SMOKE_TYPE_SMOKE
          SMOKE_TYPE_SPRAY
          SMOKE_TYPE_BUBBLES
          SMOKE_TYPE_FIRE
          SMOKE_TYPE_WATERFALL
          SMOKE_TYPE_RAIN
          SMOKE_TYPE_SNOW)
    string(REGEX MATCH "#define[ \t]+${name}[ \t]+[0-9]+" constant "${header}")
    if(NOT constant)
        message(FATAL_ERROR "Missing particle source constant: ${name}")
    endif()
    string(APPEND extracted "${constant}\n")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/effectobject.cpp"
                      "void CObject::SetupSmoke (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_particle_setup.h"
     CONTENT "${extracted}")
target_sources(xl_effect_reference PRIVATE tests/reference_particles.cpp)

file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/maths.h" math_header)
set(extracted "// Actual pinned default fixed multiplication and float rounding\n")
foreach(pattern IN ITEMS "#[ \t]*define[ \t]+XRound[^\n]+" "#define[ \t]+FixMul\\([^\n]+"
                         "inline float FRound[^\n]+")
    string(REGEX MATCH "${pattern}" part "${math_header}")
    if(NOT part)
        message(FATAL_ERROR "Missing particle fixed-math definition: ${pattern}")
    endif()
    string(APPEND extracted "${part}\n")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_particle_math.h"
     CONTENT "${extracted}")

set(extracted "// Actual pinned emitter selection and creation; backend calls are captured\n")
foreach(
    pair IN
    ITEMS "include/particles.h|PARTICLE_RAD"
          "include/particles.h|MAX_PARTICLE_SYSTEMS"
          "include/particles.h|SIMPLE_SMOKE_PARTICLES"
          "include/particles.h|SMOKE_PARTICLES"
          "include/particles.h|BUBBLE_PARTICLES"
          "include/particles.h|RAIN_PARTICLES"
          "include/particles.h|SNOW_PARTICLES"
          "include/particles.h|FIRE_PARTICLES"
          "include/particles.h|WATERFALL_PARTICLES"
          "include/objsmoke.h|STATIC_SMOKE_PART_LIFE"
          "effects/objsmoke.cpp|FIRE_PART_LIFE")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 name)
    file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" header)
    string(REGEX MATCH "#define[ \t]+${name}[ \t]+[^\n]+" constant "${header}")
    if(NOT constant)
        message(FATAL_ERROR "Missing particle emitter constant: ${name}")
    endif()
    string(APPEND extracted "${constant}\n")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_particle_constants.h"
     CONTENT "${extracted}")
set(extracted "// Actual pinned DoStaticParticles implementation\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/effects/objsmoke.cpp"
                      "void DoStaticParticles (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_particle_emitter.h"
     CONTENT "${extracted}")
set(extracted "// Actual pinned density cap and authored size scaling\n")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/particles.h" header)
foreach(signature IN ITEMS "inline int32_t MaxParticles" "inline float ParticleSize")
    string(REGEX MATCH "${signature}[^}]+}" body "${header}")
    if(NOT body)
        message(FATAL_ERROR "Missing particle size conversion: ${signature}")
    endif()
    string(APPEND extracted "${body}\n")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_particle_sizes.h"
     CONTENT "${extracted}")

foreach(
    pair IN
    ITEMS "effects/particles.cpp|fecc45b948ef2200c64052813514cfec1239e51c82130ea008de9287160f061e"
          "main/segment.cpp|eece1c8ba624bd42e6e77b9f2a3dd9a54489338139ee102245672b2b7353e60d"
          "main/side.cpp|5f8552e6fb177b24a624c47c9c644029acf95eda9f7001624c7f28e7972b4989"
          "main/findsegment.cpp|dcacfd889edf869699eb49ac8e39c4027c1da48f7c2f95eb90581bf1e7701be7"
          "maths/vecmat.cpp|4115afe844ad77a81e9f0de57ae1bbf577b06d5f49aed8b02d19002ff2021335")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Particle geometry source changed: ${path}")
    endif()
endforeach()
set(extracted "// Actual pinned face density and random surface sampling\n")
foreach(pattern IN ITEMS "#define[ \t]+X2F[^\n]+" "#define[ \t]+I2X[^\n]+")
    string(REGEX MATCH "${pattern}" part "${math_header}")
    if(NOT part)
        message(FATAL_ERROR "Missing particle coordinate definition: ${pattern}")
    endif()
    string(APPEND extracted "${part}\n")
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/main/segment.cpp" segment_source)
string(REGEX MATCH "uint16_t sideVertIndex[^;]+;" side_table "${segment_source}")
if(NOT side_table)
    message(FATAL_ERROR "Missing actual particle face table")
endif()
string(APPEND extracted "${side_table}\n")
foreach(
    signature IN
    ITEMS "inline const CFloatVector CFloatVector::Cross ("
          "inline const float CFloatVector::SqrMag (" "inline const float CFloatVector::Mag ("
          "inline CFloatVector& CFloatVector::Assign (const CFixVector&")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp" "float TriangleSize (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/segment.cpp" "float CSegment::FaceSize (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/effects/particles.cpp"
                      "CFixVector* RandomPointOnTriangle (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/effects/particles.cpp"
                      "CFixVector* RandomPointOnQuad (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/side.cpp" "void CSide::SetupCorners (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/side.cpp"
                      "CFixVector* CSide::GetCornerVertices (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_particle_geometry.h"
     CONTENT "${extracted}")
target_sources(xl_effect_reference PRIVATE tests/reference_particle_geometry.cpp)

# Execute the active world-space billboard branch, not the disabled transformed vertex alternative
# which has different corner ordering
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/effects/particles.cpp" render_source)
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/particles.h" render_header)
if(NOT render_header MATCHES "#define TRANSFORM_PARTICLE_VERTICES 0")
    message(FATAL_ERROR "Particle billboard source branch changed")
endif()
set(extracted "// Actual pinned world-space particle billboard and render type\n")
foreach(name IN ITEMS PARTICLE_TYPES SPARK_PARTICLES)
    string(REGEX MATCH "#define[ \t]+${name}[ \t]+[^\n]+" constant "${render_header}")
    if(NOT constant)
        message(FATAL_ERROR "Missing particle render constant: ${name}")
    endif()
    string(APPEND extracted "${constant}\n")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/effects/particles.cpp"
                      "int32_t CParticle::RenderType (")
string(FIND "${render_source}"
            "#else // -----------------------------------------------------------------------"
            start)
if(start LESS 0)
    message(FATAL_ERROR "Missing active world-space particle Setup branch")
endif()
string(SUBSTRING "${render_source}" ${start} -1 render_tail)
string(FIND "${render_tail}" "void CParticle::Setup (" start)
if(start LESS 0)
    message(FATAL_ERROR "Missing active particle Setup body")
endif()
string(SUBSTRING "${render_tail}" ${start} -1 render_tail)
string(FIND "${render_tail}" "\n}\n" end)
if(end LESS 0)
    message(FATAL_ERROR "Missing particle Setup end")
endif()
math(EXPR length "${end} + 3")
string(SUBSTRING "${render_tail}" 0 ${length} render_body)
string(APPEND extracted "${render_body}\n")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_particle_render.h"
     CONTENT "${extracted}")
target_sources(xl_effect_reference PRIVATE tests/reference_particle_render.cpp)

file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/particles.h" particle_header)
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/effects/particles.cpp" particle_source)
set(extracted "// Active pinned particle creation body and its visual consumers\n")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/segment.h" segment_header)
string(REGEX MATCH "#define[ \t]+SEGMENT_FUNC_SKYBOX[^\n]+" constant "${segment_header}")
if(NOT constant)
    message(FATAL_ERROR "Missing particle skybox function constant")
endif()
string(APPEND extracted "${constant}\n")
foreach(name IN ITEMS PARTICLE_TYPES PARTICLE_POSITIONS BULLET_PARTICLES LIGHT_PARTICLES
                      GATLING_PARTICLES)
    string(REGEX MATCH "#define[ \t]+${name}[ \t]+[^\n]+" constant "${particle_header}")
    if(NOT constant)
        message(FATAL_ERROR "Missing particle creation constant: ${name}")
    endif()
    string(APPEND extracted "${constant}\n")
endforeach()
string(REGEX MATCH "#[ \t]*define[ \t]+PI[^\n]+" constant "${math_header}")
string(APPEND extracted "${constant}\n")
foreach(
    pattern IN
    ITEMS "CFloatVector defaultParticleColor[^;]+;" "static int32_t smokeStartAlpha[^;]+;"
          "static int32_t brightFlags[^;]+;" "static int32_t bounceFlags[^;]+;"
          "#define RANDOM_FADE[^\n]+")
    string(REGEX MATCH "${pattern}" data "${particle_source}")
    if(NOT data)
        message(FATAL_ERROR "Missing particle creation data: ${pattern}")
    endif()
    string(APPEND extracted "${data}\n")
endforeach()
foreach(
    signature IN
    ITEMS "inline const CFloatVector CFloatVector::Min (CFloatVector& v, const float"
          "inline const CFixVector CFixVector::Avg (const CFixVector&"
          "inline const fix CFixVector::Dist ("
          "inline const fix CFixVector::DistToPlane ("
          "inline const fix CFixVector::Dot (const fix"
          "inline const fixang CFixVector::DeltaAngle (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" "${signature}")
endforeach()
foreach(
    signature IN
    ITEMS "static inline int32_t RandN ("
          "inline float sqr ("
          "static inline float Luminance ("
          "static inline float SmokeStartAlpha ("
          "int32_t CParticle::Create ("
          "void CParticle::UpdateDecay ("
          "void CParticle::UpdateTexCoord ("
          "int32_t CParticle::SetupColor ("
          "int32_t CParticle::RenderType ("
          "inline int32_t CParticle::ChangeDir ("
          "fix CParticle::Drag ("
          "int32_t CParticle::Bounce ("
          "int32_t CParticle::UpdateDrift ("
          "void CParticle::UpdateColor ("
          "int32_t CParticle::Update (")
    # Update contains a column-zero brace in an inactive conditional branch Bound whole particle
    # functions by the source's inter-function separator
    string(FIND "${particle_source}" "${signature}" start)
    if(start LESS 0)
        message(FATAL_ERROR "Missing particle function: ${signature}")
    endif()
    string(SUBSTRING "${particle_source}" ${start} -1 tail)
    string(FIND "${tail}" "\n//----------------" end)
    if(end LESS 0)
        message(FATAL_ERROR "Missing particle function separator: ${signature}")
    endif()
    string(SUBSTRING "${tail}" 0 ${end} body)
    if(NOT body MATCHES "\n}[ \t\n]*$")
        message(FATAL_ERROR "Incomplete particle function: ${signature}")
    endif()
    string(APPEND extracted "${body}\n")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_particle_create.h"
     CONTENT "${extracted}")

set(extracted "// Actual source point containment, exhaustive fallback and particle collision\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/side.cpp" "CSegMasks CSide::Masks (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/segment.cpp"
                      "CSegMasks CSegment::Masks (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/findsegment.cpp" "int32_t PointInSeg (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/findsegment.cpp"
                      "int32_t FindSegByPosExhaustive (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/effects/particles.cpp"
                      "int32_t CParticle::CollideWithWall (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_particle_world.h"
     CONTENT "${extracted}")
target_sources(xl_effect_reference PRIVATE tests/reference_particle_world.cpp)

file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/main/segmath.cpp" sort_hash)
if(NOT sort_hash STREQUAL "65949a1ce9a0a001ccb4a474b7b009d41e9cf0ea6975376350418d85ed1b6f4c")
    message(FATAL_ERROR "Particle normal ordering source changed")
endif()
set(extracted "// Pinned side topology, fixed normal setup and segment radii\n")
foreach(name IN ITEMS F2X I2X X2I FixDiv)
    string(REGEX MATCH "#define[ \t]+${name}\\([^\n]+" definition "${math_header}")
    if(NOT definition)
        message(FATAL_ERROR "Missing particle world fixed conversion: ${name}")
    endif()
    string(APPEND extracted "${definition}\n")
endforeach()
string(APPEND extracted "#define XRound(value) (fix)(value)\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/segmath.cpp"
                      "uint16_t SortVertsForNormal (")
foreach(
    signature IN
    ITEMS "void CSide::SetupCorners ("
          "void CSide::SetupFaceVertIndex ("
          "void CSide::SetupVertexList ("
          "void CSide::SetupAsTriangles ("
          "fix CSide::Height ("
          "bool CSide::IsPlanar ("
          "uint8_t CSide::Dist (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/side.cpp" "${signature}")
endforeach()
# Supplied-geometry cases retain their explicit planar flag; compiled cases use the actual body
string(REPLACE "bool CSide::IsPlanar (" "bool CSide::ComputedPlanar (" extracted "${extracted}")
foreach(signature IN ITEMS "void CSegment::ComputeCenter (" "void CSegment::ComputeRads (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/segment.cpp" "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h"
                      "inline CFixVector& CFixVector::Assign (const CFloatVector&")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_particle_world_setup.h"
     CONTENT "${extracted}")

file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/main/gamedata.cpp" grid_hash)
if(NOT grid_hash STREQUAL "782acd85fae6fa5fbb0162a9f05474ae449063d70bf566d172c9d47ae4ed1a12")
    message(FATAL_ERROR "Particle spatial grid source changed")
endif()
set(extracted "// Pinned fixed-grid construction and candidate lookup\ninline fix Ceil(fix);\n")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/main/gamedata.cpp" grid_source)
string(REGEX MATCH "#define GRID_INDEX[^\n]+" grid_macro "${grid_source}")
if(NOT grid_macro)
    message(FATAL_ERROR "Missing source spatial grid index")
endif()
string(APPEND extracted "${grid_macro}\n")
foreach(
    signature IN
    ITEMS "inline fix Floor ("
          "inline fix Ceil (fix v)\n{"
          "inline CFixVector& Floor ("
          "inline CFixVector& Ceil ("
          "inline CFixVector& ToInt ("
          "inline int32_t CSegmentGrid::GridIndex ("
          "bool CSegmentGrid::Create ("
          "void CSegmentGrid::Destroy ("
          "int32_t CSegmentGrid::GetSegList (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/gamedata.cpp" "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h"
                      "inline const CFixVector& CFixVector::operator/= (const fix")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_particle_grid.h"
     CONTENT "${extracted}")

file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/maths/tables.cpp" math_tables)
string(REGEX MATCH "uint8_t guess_table \\[\\] = \\{[^}]*\\};" guesses "${math_tables}")
if(NOT guesses)
    message(FATAL_ERROR "Missing actual integer square-root table")
endif()
set(extracted "// Pinned setup-format integer magnitude and fixed normal\n${guesses}\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/fix.h"
                      "static inline void FixQuadNegate (")
foreach(signature IN ITEMS "void FixMulAccum (" "uint32_t FixDivQuadLongU (" "uint16_t LongSqrt ("
                           "uint32_t QuadSqrt (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/fixc.cpp" "${signature}")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_particle_integer.h"
     CONTENT "${extracted}")
set(extracted "// Pinned fixed normal construction, including source edge scaling\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/maths/vecmat.cpp" "void CFixVector::Check (")
foreach(signature IN ITEMS "inline const CFixVector CFixVector::Perp ("
                           "inline const CFixVector CFixVector::Normal (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h" "${signature}")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_particle_normal.h"
     CONTENT "${extracted}")

file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/effects/particle_emitter.cpp" emitter_hash)
if(NOT emitter_hash STREQUAL "0a92df8997332d46abc2132b3f91e5831958d0a63ac64a8cdf6594318d7ae3a5")
    message(FATAL_ERROR "Particle emitter source changed")
endif()
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/include/carray.h" emitter_array_hash)
if(NOT emitter_array_hash STREQUAL
   "10ea5f725a079e9be98f7871ffa6544d1de5c427db472b214b3f91188719019c")
    message(FATAL_ERROR "Particle emitter array semantics changed")
endif()
set(extracted "// Pinned emitter allocation, scheduling, ring retirement and disabling\n")
foreach(signature IN ITEMS "int32_t CParticleEmitter::Create ("
                           "int32_t CParticleEmitter::Update (" "void CParticleEmitter::SetLife (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/effects/particle_emitter.cpp"
                          "${signature}")
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/particles.h" emitter_header)
string(REGEX MATCH "inline bool IsAlive \\([^}]+}" alive_body "${emitter_header}")
if(NOT alive_body)
    message(FATAL_ERROR "Missing particle emitter lifetime predicate")
endif()
string(REPLACE "inline bool IsAlive" "inline bool CParticleEmitter::IsAlive" alive_body
               "${alive_body}")
string(APPEND extracted "${alive_body}\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/vecmat.h"
                      "inline CFixVector& CFixVector::Assign (const CFloatVector&")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_fire_emitter.h"
     CONTENT "${extracted}")
