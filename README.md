# Card Pulse

<img width="963" height="739" alt="Render" src="https://github.com/user-attachments/assets/7c82bd2a-8b0e-43c6-9d4e-fb3f919496d3" />

A little TCG display that can hold a card in a slab that includes a screen to display the value of the card, and a rotary encoder and momentary push button to control the screen. Ws2812bs surround the card in a rectangular frame to surround the card with a underglow and light some transparent filament on the front cover. I'm aiming for the final version to include a wifi menu, a LED control menu, a card price home screen, and a price graph. I'm thinking the rotary encoder is used for selecting options, and the button is for clicking, or pressing OK.

## Case Summary

The card itself will be stored inside a protective 35pt case while inside a Card Pulse. The lid is screwed in via 3 m3*12 screws, and doing so will secure the toploader in place. I'm also planning on making a version that uses magnets. The shell will be printed in black, and the lid will be printed with both black and white. The only part that requires an AMS or some kind of filament changing system is the pokeball knob, which can just be printed in one full color, maybe red or white. The case has 2 main chambers, a battery chamber, and a microcontroller/screen chamber. The battery chamber sits at the top of the card frame, north of the actual card and LEDS, and on its left and right are 2 parts: a 5v boost, and a power switch. The microcontroller/screen chamber is where the screen, esp32s3, rotary encoder, and 12mm momentary push button all sit. The rotary encoder, button, and screen are all secured by the lid, and the microcontroller is secured in place via the USBC hole and maybe some zip ties. The LEDs form a rectangular outline around the card, and provide both a glow for the indicators in the lid, and a underglow that surrounds the card. The leds will all be tied together, and the wires will go down back to the controller via a wire channel. The channel will be another rectangular outline, this time surrounding the LEDs, and it will basically enclose all the wires that are related to the battery and xiao.

## Complete List of Parts

1. ESP32S3
2. KY040
3. 12 mm Momentary Push Button
4. 18650 3.7 V Lithium battery
5. 2.42 in OLED Screen
6. SN74AHCT125N Level Shifter
7. 5V Boost
8. KCD01
9. Resistor: 330 R
10. Capacitor: 1000uf
11. Capacitor  0.1uf

