FROM debian:bullseye

COPY ./requirements.txt /mnt/

RUN apt update && \
    DEBIAN_FRONTEND=noninteractive apt install -y build-essential postgresql libpq-dev libwrap0-dev python3 nano cython3 python3-pip openssh-client rsync libssl-dev && \
    pip install pyinstaller && \
    pip install -r /mnt/requirements.txt

COPY ./docker-entrypoint.sh /

RUN chmod +x /docker-entrypoint.sh

WORKDIR /mnt/regress/


ENTRYPOINT ["/docker-entrypoint.sh"]
# To compile:
    # Add Makefile rule:
        # async:
	    #   rsync -av -i ../bacula/src/ build/src/
    # Change bacula directory in config file to: /mnt/regress/
    # Execute:
    #    make async && -C build/src/plugins/fd install-kubernetes && make -C build/src/plugins/fd/kubernetes-backend clean && make -C build/src/plugins/fd/kubernetes-backend install-kubernetes
    # Binary in: ~/regress/bin/k8s_backend