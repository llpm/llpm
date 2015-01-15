#/bin/bash

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

docker run --rm -it -v ${DIR}/../:/src llpm
