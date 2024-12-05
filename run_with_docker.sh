#!/bin/bash

docker compose up -d --build

if [[ $? != 0 ]]
then
	echo "Nu s-a putut crea/porni containerul Docker"
	docker compose down
	cd ..
	exit
fi

docker exec -w /apd/checker apd_container sh -c "for i in \$(seq 1 100); do /apd/checker/checker.sh | tail -n3; sleep 3; done"
docker compose down
cd ..
