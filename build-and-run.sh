set -e

cd /workspaces/cuda-quantum
source markus-login.env

cd /workspaces/cuda-quantum/scripts/build
ninja install -j3
cd /workspaces/cuda-quantum/scripts/build
cd /workspaces/cuda-quantum/examples/cpp/providers

echo "compile done. build fermioniq.cpp"

RC=95ab08ab-7b8c-480a-914b-c1206dd6f373
PROJECT_ID=943977db-7264-4b66-addf-c9d6085d9d8f

nvq++ fermioniq.cpp \
    --target fermioniq \
    --fermioniq-remote-config $RC \
    --fermioniq-bond-dim 3 \
    --fermioniq-project-id $PROJECT_ID

echo "build done. Run"

export CUDAQ_LOG_LEVEL=debug

./a.out x