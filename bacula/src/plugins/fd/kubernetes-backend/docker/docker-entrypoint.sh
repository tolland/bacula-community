#!/bin/bash

TMP_DIR="/mnt/regress/tmp"

if [ ! -d "$TMP_DIR" ]; then
    make setup
else
    echo "The directory /mnt/regress/tmp already exists. So, we do not make setup"
fi

/bin/bash
