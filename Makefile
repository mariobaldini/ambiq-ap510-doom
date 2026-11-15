# Host entry point. Everything except docker itself runs inside the build container, as the host user.

DOCKER ?= docker
IMAGE  := ambiq-ap510-doom:$(shell sha256sum tools/Dockerfile | cut -c1-12)
OS_ID  ?= $(shell . /etc/os-release 2>/dev/null; echo "$$ID $$ID_LIKE")
JOBS   ?= $(shell nproc)

DOOM_ARGS ?=
JLINK_SN  ?=

RUN := $(DOCKER) run --rm --init --user $(shell id -u):$(shell id -g) -e HOME=/work/build/.home \
       -v "$(CURDIR)":/work -w /work --security-opt label=disable
USB := -v /dev/bus/usb:/dev/bus/usb --device-cgroup-rule='c 189:* rmw'
TTY := $(shell test -t 0 && echo -it)
FW  := make -f firmware.mk --no-print-directory DOOM_ARGS='$(DOOM_ARGS)' JLINK_SN='$(JLINK_SN)'

.PHONY: help setup build flash reset swo clean check-docker check-udev image

help:
	@echo "make setup       check docker + J-Link udev rule, build the image, then build"
	@echo "make build       firmware + WAD into build/ (fetches submodules, toolchain, WAD on first run)"
	@echo "make flash       build, then program the EVB (app + WAD)"
	@echo "make reset       reset the board"
	@echo "make swo         show the SWO log (Ctrl-C to stop)"
	@echo "make clean       delete build/"
	@echo "options: DOOM_ARGS='-timedemo demo1'  JLINK_SN=<probe serial>"

setup: check-docker check-udev image build

check-docker:
	@if ! command -v $(DOCKER) >/dev/null 2>&1; then \
		echo "docker is not installed. Install it, then log out and back in:"; \
		$(MAKE) --no-print-directory -s docker-install-help; exit 1; \
	elif ! $(DOCKER) info >/dev/null 2>&1; then \
		echo "docker is installed but cannot run without sudo. Run, then log out and back in:"; \
		echo "  sudo systemctl enable --now docker"; \
		echo "  sudo usermod -aG docker $$USER"; \
		exit 1; \
	fi

.PHONY: docker-install-help
docker-install-help:
	@case " $(OS_ID) " in \
	*" fedora "*|*" rhel "*) \
		echo "  sudo dnf -y install dnf-plugins-core"; \
		echo "  sudo dnf config-manager addrepo --from-repofile=https://download.docker.com/linux/fedora/docker-ce.repo"; \
		echo "  sudo dnf -y install docker-ce docker-ce-cli containerd.io docker-buildx-plugin"; \
		echo "  sudo systemctl enable --now docker"; \
		echo "  sudo usermod -aG docker $$USER";; \
	*" ubuntu "*|*" debian "*) \
		echo "  sudo apt-get update"; \
		echo "  sudo apt-get install -y docker.io docker-buildx"; \
		echo "  sudo systemctl enable --now docker"; \
		echo "  sudo usermod -aG docker $$USER";; \
	*) echo "  see https://docs.docker.com/engine/install/ (distro '$(OS_ID)' not recognized)";; \
	esac

check-udev:
	@if ! grep -qs '1366' /etc/udev/rules.d/*.rules /usr/lib/udev/rules.d/*.rules /lib/udev/rules.d/*.rules; then \
		echo "warning: no udev rule for SEGGER J-Link (USB 1366); make flash will fail. Add it with:"; \
		echo "  echo 'SUBSYSTEM==\"usb\", ATTR{idVendor}==\"1366\", MODE=\"0666\"' | sudo tee /etc/udev/rules.d/99-jlink.rules"; \
		echo "  sudo udevadm control --reload-rules && sudo udevadm trigger"; \
	fi

image: check-docker
	@$(DOCKER) image inspect $(IMAGE) >/dev/null 2>&1 || $(DOCKER) build -t $(IMAGE) -f tools/Dockerfile tools

build: image
	@mkdir -p build/.home
	$(RUN) $(IMAGE) $(FW) -j$(JOBS) all

flash: image
	@mkdir -p build/.home
	$(RUN) $(USB) $(IMAGE) $(FW) -j$(JOBS) flash

reset: image
	@mkdir -p build/.home
	$(RUN) $(USB) $(IMAGE) $(FW) reset

swo: image
	@mkdir -p build/.home
	$(RUN) $(TTY) $(USB) $(IMAGE) $(FW) swo

clean:
	rm -rf build
