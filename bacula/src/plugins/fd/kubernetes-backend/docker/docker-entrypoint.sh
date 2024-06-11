#!/bin/bash

TMP_DIR="/mnt/regress/tmp"

if [ ! -d "$TMP_DIR" ]; then
    make setup
    cat <<EOF >/mnt/regress/k8s_compile
#!/bin/bash
make async
printf "\n%s\n" "Make async done."
make -C build/src/plugins/fd install-kubernetes
printf "\n%s\n" "Compiled k8s fd."
make -C build/src/plugins/fd/kubernetes-backend clean
printf "\n%s\n" "Cleaned k8s binary."
make -C build/src/plugins/fd/kubernetes-backend install-kubernetes
printf "\n%s\n" "Compiled k8s binary."
EOF
chmod +x /mnt/regress/k8s_compile
else
    echo "The directory /mnt/regress/tmp already exists. So, we do not make setup"
fi

/bin/bash
