cd $(dirname $0)/../../
echo "Gen JNI header dir: $(pwd)"

DEP_JAR=$1
CLIENT_DIR=$2

set -x
javah -cp ${DEP_JAR}:${CLIENT_DIR} -o ./src/jni/oblogreader_jni.h -jni com.oceanbase.logclient.LibOblogReaderJni
