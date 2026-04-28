PIO ?= pio
ENV ?= lilygo-t-display-v1-1
BAUD ?= 115200
PORT ?= $(shell ls /dev/tty.usbmodem* /dev/tty.usbserial* /dev/cu.usbmodem* /dev/cu.usbserial* 2>/dev/null | head -n 1)

.PHONY: help build clean monitor upload release check-pio check-port

help:
	@echo "Targets:"
	@echo "  make build            - Build firmware"
	@echo "  make clean            - Clean build artifacts"
	@echo "  make monitor          - Open serial monitor via USB port"
	@echo "  make upload CONFIRM=1 - Upload firmware via USB (requires confirmation flag)"
	@echo "  make release          - Build + create GitHub release for OTA"
	@echo ""
	@echo "Variables:"
	@echo "  ENV=<platformio_env>  (default: $(ENV))"
	@echo "  BAUD=<monitor_baud>   (default: $(BAUD))"
	@echo "  PORT=<usb_serial_port> (auto-detected by default)"
	@echo "  DRY_RUN=1             (for make release: no tag/push/release)"

check-pio:
	@command -v $(PIO) >/dev/null 2>&1 || { echo "Error: '$(PIO)' is not installed."; exit 1; }

check-port:
	@if [ -z "$(PORT)" ]; then \
		echo "Error: USB serial port was not auto-detected."; \
		echo "Connect the board or pass PORT manually, e.g.:"; \
		echo "  make monitor PORT=/dev/tty.usbmodem1101"; \
		exit 1; \
	fi

build: check-pio
	$(PIO) run -e $(ENV)

clean: check-pio
	$(PIO) run -e $(ENV) -t clean

monitor: check-pio check-port
	$(PIO) device monitor -p "$(PORT)" -b $(BAUD)

upload: check-pio check-port
	@if [ "$(CONFIRM)" != "1" ]; then \
		echo "Upload is protected."; \
		echo "Run: make upload CONFIRM=1"; \
		exit 2; \
	fi
	$(PIO) run -e $(ENV) -t upload --upload-port "$(PORT)"

release: check-pio
	bash ./scripts/release.sh
