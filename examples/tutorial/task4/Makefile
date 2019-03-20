# name of your application
APPLICATION = Task04

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../../

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
CFLAGS += -g -DDEVELHELP -DXTIMER_HZ=32000000 -DQEMU -O0

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

# Modules to include:
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps
USEMODULE += gnrc_pktdump
USEMODULE += gnrc_txtsnd
USEMODULE += gnrc_netdev_default
USEMODULE += auto_init_gnrc_netif
USEMODULE += stellaris_ether
USEMODULE += netdev_eth
USEMODULE += netstats_l2
include $(RIOTBASE)/Makefile.include

