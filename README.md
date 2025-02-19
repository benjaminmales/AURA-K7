# KKM K7P Demo Firmware

## Building
1. Create a workspace for this project (e.g., `mkdir /path/to/k7pworkspace`)
2. Clone this repo into the workspace. Assuming using default name so it will be in a folder called `aura-k7p`
3. Initialise the workspace: `west init -l /path/to/k7pworkspace/aura-k7`
4. Download dependencies: `west update`
5. From workspace root build with: `west build-b kkm_k7p -s aura-k7/app`


## Flashing
1. Connect to `VDD` (`VTG`), `GND` (`GND`), `DIO` (`SWDIO`) and `CLK` (`SWDCLK`) signals to programmer/dev board.
Dev board signal names are in brackets.
2. `west flash`

```
% west flash
-- west flash: rebuilding
[0/5] Performing build step for 'app'
[7/7] Linking C executable zephyr/zephyr.elf
Memory region         Used Size  Region Size  %age Used
           FLASH:      223532 B       508 KB     42.97%
             RAM:       74276 B       128 KB     56.67%
        IDT_LIST:          0 GB        32 KB      0.00%
Generating files from /workspace/build/app/zephyr/zephyr.elf for board: kkm_k7p
[5/5] Generating ../merged.hex
-- west flash: using runner nrfjprog
-- runners.nrfjprog: reset after flashing requested
Using board 683149788
-- runners.nrfjprog: Flashing file: /workspace/build/merged.hex
[ #################### ]   5.901s | Erase file - Done erasing
[ #################### ]   2.745s | Program file - Done programming
[ #################### ]   2.704s | Verify file - Done verifying
Applying system reset.
Run.
```
