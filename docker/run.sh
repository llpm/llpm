#/bin/bash

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

docker build -t llpm ${DIR}
docker run --rm -it -v ${DIR}/../:/src llpm
