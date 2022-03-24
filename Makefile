.PHONY: all clean sdout vax vboot vloader

all: vax vboot vloader sdout

clean:
	@$(MAKE) clean -C vax
	@$(MAKE) clean -C vboot
	@$(MAKE) clean -C vloader
	@rm -rf SdOut

sdout:
	@mkdir -p SdOut/vax
	@cp vboot/vboot.elf SdOut/vax/vboot.elf
	@cp vloader/vloader.elf SdOut/vax/vloader.elf
	@cp vloader/vloader.elf SdOut/vax/vloader.elf
	@mkdir -p SdOut/atmosphere/contents/0100000000000BED/flags
	@touch SdOut/atmosphere/contents/0100000000000BED/flags/boot2.flag
	@cp vax/vax.nsp SdOut/atmosphere/contents/0100000000000BED/exefs.nsp

vax:
	@$(MAKE) -C vax

vboot:
	@$(MAKE) -C vboot

vloader:
	@$(MAKE) -C vloader