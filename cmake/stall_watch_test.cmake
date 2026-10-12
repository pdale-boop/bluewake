add_executable(bluewake_stall_watch_test "${BLUEWAKE_REPO_ROOT}/tests/stall_watch_test.c")
target_include_directories(bluewake_stall_watch_test PRIVATE "${BLUEWAKE_HOST_SRC}")
add_test(NAME bluewake_stall_watch_test COMMAND bluewake_stall_watch_test)
