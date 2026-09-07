# Card Pulse

<img width="963" height="739" alt="Render" src="https://github.com/user-attachments/assets/7c82bd2a-8b0e-43c6-9d4e-fb3f919496d3" />

A little TCG display that can hold a card in a slab that includes a screen to display the value of the card, and a rotary encoder and momentary push button to control the screen. Ws2812bs surround the card in a rectangular frame to surround the card with a underglow and light some transparent filament on the front cover. I'm aiming for the final version to include a wifi menu, a LED control menu, a card price home screen, and a price graph. I'm thinking the rotary encoder is used for selecting options, and the button is for clicking, or pressing OK.

## Case Summary

The card will be inserted into a protective 35pt Toploader, and inserted into the base shell, and then held in place by the top lid, which is screwed in to the base via screws. The case has many different paths and spaces for the different parts:

The LEDs have a rectangular cutout around the card, with small holes in the wall separating them to make a underglow effect, but will be also facing upwards, into 3D printed diffusers to have status leds in the lid.

The case has a second rectangular hole surrounding the leds, I’m planning on putting all the battery and led wires in it (the battery is above the leds, and therefore the leds are in between the battery and xiao).The wire paths will have an occasional hole in the intersecting wall to provide holes for wires to come through.

The card also has a space for batteries, already mentioned above, it sits at the top of the case. It has a space next to it for the 5V boost for leds, and it has a slot to the other side for the power switch.

## Complete List of Parts

1. ESP32S3
2. KY040
3. 12 mm Momentary Push Button
4. 18650 3.7 V Lithium battery
5. 2.42 in Oled Screen
6. 74AHCT14 Level Shifter
7. 5V Boost
8. KCD01
9. Resistor: 330 R
10. Capacitor: 1000uf

