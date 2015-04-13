#! /bin/bash

for file in $(ls -R); do
	res=$(grep "define XOR" $file)
	if [[ ! ($res == "") ]]; then
		echo "File name: $file"
		echo $res
	fi
done
