#!/usr/bin/env bats   -*- bats -*-
#
# Tests podman system service CORS enabled
#

load helpers
load helpers.network

SERVICE_NAME="podman_test_$(random_string)"

SERVICE_TCP_HOST="localhost"

SERVICE_FILE="$UNIT_DIR/$SERVICE_NAME.service"
SOCKET_FILE="$UNIT_DIR/$SERVICE_NAME.socket"

# bats test_tags=ci:parallel
@test "podman system service - tcp CORS" {
    skip_if_remote "system service tests are meaningless over remote"
    PORT=$(random_free_port 63000-64999)
    log=${PODMAN_TMPDIR}/system-service.log
    $PODMAN system service --cors="*" tcp:$SERVICE_TCP_HOST:$PORT -t 20 2> $log &
    podman_pid="$!"
    sleep 5s
    run curl -s --max-time 10 -vvv $SERVICE_TCP_HOST:$PORT/_ping 2>&1
    is "$output" ".*< Access-Control-Allow-Origin: \*.*" "access-control-allow-origin verifies CORS is set"
    kill $podman_pid
    wait $podman_pid || true

    # Running server over TCP is a bad idea. We should see a warning
    assert "$(< $log)" =~ "Using the Podman API service with TCP sockets" \
           "podman warns about server on TCP"
}

# bats test_tags=ci:parallel
@test "podman system service - tcp without CORS" {
    skip_if_remote "system service tests are meaningless over remote"
    PORT=$(random_free_port 63000-64999)
    $PODMAN system service tcp:$SERVICE_TCP_HOST:$PORT -t 20 &
    podman_pid="$!"
    sleep 5s
    (curl -s --max-time 10 -vvv $SERVICE_TCP_HOST:$PORT/_ping 2>&1 | grep -Eq "Access-Control-Allow-Origin:") && false || true
    kill $podman_pid
    wait $podman_pid || true
}

# bats test_tags=ci:parallel
@test "podman system service - CORS enabled in logs" {
    skip_if_remote "system service tests are meaningless over remote"

    PORT=$(random_free_port 63000-64999)
    run_podman 0+w system service --log-level="debug" --cors="*" -t 1 tcp:$SERVICE_TCP_HOST:$PORT
    is "$output" ".*CORS Headers were set to ..\*...*" "debug log confirms CORS headers set"
    assert "$output" =~ "level=warning msg=\"Using the Podman API service with TCP sockets is not recommended" \
           "TCP socket warning"
}

# vim: filetype=sh
