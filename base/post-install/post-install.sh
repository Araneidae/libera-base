#!/bin/sh

# Post installation customisation script for DLS Libera.  This is called at
# the end of system startup with the rootfs mounted in writeable mode.

cd "$(dirname "$0")"

SCRIPT_ARGS="$1"

INSTALL_BASE=/mnt/nfs/testing/libera-upgrade
LIBERA_BASE=/mnt/nfs/testing/ioc
PLACE="$SCRIPT_ARGS"


# Get ntp up and running as soon as possible so we have sensible timestamps.
cp ntp.conf /etc
/etc/init.d/ntpd restart



# Prepare the NFS mount points
mkdir -p /mnt/nfs /mnt/prod /mnt/work
# Install our network specific files.
cp resolv.conf.* fstab.* /etc
# Install the ssh keys.
cp authorized_keys /root/.ssh

# Configure our network appropriately.

# Now set up the resolv.conf and extra mounts.  We need to identify our
# network at this point.
BROADCAST="$(
    ifconfig eth0 |
    sed -nr '/^eth0/{n; s/.*Bcast:([^ ]*) .*/\1/;p;q;}')"
case "$BROADCAST" in
    172.23.255.255)   network=lab ;;
    172.23.207.255)   network=pri ;;
    *)  Error 'Do not recognise network, resolv.conf not configured.'
esac

echo "Configuring resolv.conf and local mounts for $network"
echo "FSTABS=/etc/fstab.$network" >/etc/mount-extra
ln -s resolv.conf.$network /etc/resolv.conf


# Mount the nfs mount points so we can do the rest of the install.
/etc/init.d/mount-extra start

# Finally we can install Libera base and perform the final upgrade.  We need
# the following parameters:
#   - location of the install-base command
#   - location of the Libera ioc
#   - type identifier: SR or BO
echo Installing Libera base from "$INSTALL_BASE"
"$INSTALL_BASE"/base/install-base $PLACE
echo Installing Libera from "$LIBERA_BASE"
[ -e /mnt/nfs/state/$(hostname).state ]  &&
    cp /mnt/nfs/state/$(hostname).state ]  /opt/state
"$LIBERA_BASE"/install_d/libera-install-ioc -fas $PLACE
