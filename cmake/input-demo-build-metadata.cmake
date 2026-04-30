include_guard(GLOBAL)

find_package(Git QUIET)

function(dxx_input_demo_get_build_metadata out_build_number out_git_version)
	set(build_number "0")
	set(git_version "unknown")
	set(repo_root "${CMAKE_CURRENT_LIST_DIR}/..")

	if(DEFINED DXX_INPUT_DEMO_BUILD_NUMBERi AND NOT "${DXX_INPUT_DEMO_BUILD_NUMBERi}" STREQUAL "")
		set(build_number "${DXX_INPUT_DEMO_BUILD_NUMBERi}")
	elseif(GIT_FOUND)
		execute_process(
			COMMAND "${GIT_EXECUTABLE}" rev-list --count HEAD
			WORKING_DIRECTORY "${repo_root}"
			OUTPUT_VARIABLE git_commit_count
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET)
		if(git_commit_count MATCHES "^[0-9]+$")
			math(EXPR build_number "${git_commit_count} * 10")
		endif()
	endif()

	if(DEFINED DXX_INPUT_DEMO_GIT_VERSION AND NOT "${DXX_INPUT_DEMO_GIT_VERSION}" STREQUAL "")
		set(git_version "${DXX_INPUT_DEMO_GIT_VERSION}")
	elseif(GIT_FOUND)
		execute_process(
			COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
			WORKING_DIRECTORY "${repo_root}"
			OUTPUT_VARIABLE git_short_hash
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET)
		if(NOT "${git_short_hash}" STREQUAL "")
			set(git_version "${git_short_hash}")
		endif()
	endif()

	set(${out_build_number} "${build_number}" PARENT_SCOPE)
	set(${out_git_version} "${git_version}" PARENT_SCOPE)
endfunction()

function(dxx_input_demo_apply_build_metadata target)
	dxx_input_demo_get_build_metadata(build_number git_version)
	string(REPLACE "\\" "\\\\" escaped_git_version "${git_version}")
	string(REPLACE "\"" "\\\"" escaped_git_version "${escaped_git_version}")
	target_compile_definitions(${target} PRIVATE
		DXX_INPUT_DEMO_BUILD_NUMBERi=${build_number}
		DXX_INPUT_DEMO_GIT_VERSION=\"${escaped_git_version}\")
endfunction()