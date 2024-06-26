set (TEST_USE_BINARIES RAFTKEEPER_TESTS_SERVER_BIN_PATH=${RaftKeeper_BINARY_DIR}/programs/raftkeeper RAFTKEEPER_TESTS_CLIENT_BIN_PATH=${RaftKeeper_BINARY_DIR}/programs/raftkeeper)

find_program(DOCKER_CMD docker)
find_program(DOCKER_COMPOSE_CMD docker-compose)
find_program(PYTEST_CMD pytest)
find_program(SUDO_CMD sudo)

# will mount only one binary to docker container - build with .so cant work
if(DOCKER_CMD)
    if(INTEGRATION_USE_RUNNER AND SUDO_CMD)
        add_test(NAME integration-runner WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND ${SUDO_CMD} ${CMAKE_CURRENT_SOURCE_DIR}/runner --binary ${RaftKeeper_BINARY_DIR}/programs/raftkeeper --configs-dir ${RaftKeeper_SOURCE_DIR}/programs/server/)
        message(STATUS "Using tests in docker with runner SUDO=${SUDO_CMD}; DOCKER=${DOCKER_CMD};")
    endif()
    if(NOT INTEGRATION_USE_RUNNER AND DOCKER_COMPOSE_CMD AND PYTEST_CMD)
        # To run one test with debug:
        # cmake . -DPYTEST_OPT="-ss;test_cluster_copier"
        add_test(NAME integration-pytest WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND env ${TEST_USE_BINARIES} "RAFTKEEPER_TESTS_BASE_CONFIG_DIR=${RaftKeeper_SOURCE_DIR}/programs/server/" "RAFTKEEPER_TESTS_CONFIG_DIR=${RaftKeeper_SOURCE_DIR}/tests/config/" ${PYTEST_STARTER} ${PYTEST_CMD} ${PYTEST_OPT})
        message(STATUS "Using tests in docker DOCKER=${DOCKER_CMD}; DOCKER_COMPOSE=${DOCKER_COMPOSE_CMD}; PYTEST=${PYTEST_STARTER} ${PYTEST_CMD} ${PYTEST_OPT}")
    endif()
endif()
