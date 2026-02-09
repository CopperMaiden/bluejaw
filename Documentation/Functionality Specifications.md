
# Feature-set (MVP)
- Bluetooth audio sharing
	- Single host (audio input) to multiple clients (audio output)
	- Basic media controls (Pausing, play, rewind, skip, volume)
	- Transfer media player metadata for display
	- Individual client device volume adjustment
	- Source volume adjustment
	- EQ Adjustment (low, mid, high (Make some frequencies louder than others))
	- Resync clients option (Set to automatic with user specified time interval or manual resync)
	
- Cute as &#$@ display (User interface)
	- 128x64 pixels
	- Expect 60-120 fps
	- Animation toggle between "cutesy" and "HARDCOREEE" (Playful menu item names) (fluid animations on or off)
	- Record with like pulsing graphics on title screen (default screen)
	- Primary screen should idle on the title screen (music details and basic controls)
	- Secondary screen reserved for settings with multiple submenus
	- Battery percentage displayed at top left corner at all times
	- Top menu bar for device specific data: battery, bluetooth connectivity status, etc.
	- Settings includes category to select individual client device settings
	- Progress bar for music (Customizable, IF time allows during development)
	
- The dial of doom AKA "D.O.D." (Rotary encoder)
	- Volume adjustment between 0-11
	- Volume displayed as a number with other device specific data at top right
	- Used for all menu interactions and value adjustments, as well as device power on and off states 
	
- QOL suggestions (Implement as time allows):
	- Volume increment control (Allows user to fine tune volume increments of the 0-11 scale)
	- Battery saving idle screen (Toggleable in settings, if enabled, screen turns off after 10 seconds of inactivity and wakes with a press of the encoder, time is adjustable)
	- When on very, very low battery, overly dramatic countdown starts until the device powers off
	- Third "custom" option for animations/transitions to toggle specific animations as desired
	- Resyncing has a symbol at the top menu bar when active
	- Individual client delay compensation (Client setting to add a delay for slower devices)
	- ! SYNCING WILL BE REQUIRED FOR I2S, UART, AND BLUETOOTH !