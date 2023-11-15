set -x

cd $(dirname $0)/..
dir=$(pwd)
protoc -I. --java_out=${dir}/common/src/main/java proto/logproxy.proto
protoc -I. --cpp_out=${dir}/src proto/logproxy.proto
