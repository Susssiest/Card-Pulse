# Journal

## August 29th: Worked on V1

Started working on the 3d printed frame. Main idea is to have a stand that can plug into a wall outlet and display the led and card price. Going to have leds on the sides, and a oled screen to display the price. Using blender, experiencing the usual problems, non manifold vertices, overlapping vertices, etc. Using ws2812b leds for the bars. I am probably going to use a standard 35pt toploader, but I might make one for psa card slabs. Imported reference card to make it more realistic.

<img width="708" height="695" alt="Screenshot 2026-09-04 at 1 29 05 PM" src="https://github.com/user-attachments/assets/afa9da40-ef14-4780-aedc-7c852ac5f158" />

**Total time spent: 3 hours**

## August 31st: Researched and scoured the internet for 3d models of parts.

I spent some time reading articles and forums, as well as talking to Claude for a little. The final parts are going to be:

- KY040
- 12 mm Momentary push button
- Seeed Xiao ESP32C3
- 5V boost
- WS2812B strips
- 3.42 in Oled Screen
- 3.7 V 18650 Lithium Battery
- Wires

  I spent forever scouring the internet for 3D models of the parts, I found most on GrabCad, but a few took forever to find. I also kept on changing my mind between using a KY040 and EC11, but I finally chose the KY040 becuase the pins dont extrude downwards and it is more compact.

<img width="620" height="221" alt="Screenshot 2026-09-04 at 6 17 11 PM" src="https://github.com/user-attachments/assets/567e627d-e8c4-4561-b3ac-c6c3026a32e7" />

**Total time spent: 40 minutes**

## September 2nd: Worked on V2

  I kind of think the frame looks ugly, so I'm restarting. This time I'm thinking of actually having it mobile, and more like a portable case than a frame that sits on your shelf. I will be adding the 3.7 v lithium battery, and a 5v booster, momentary push button, and rotary enocoder into the shell of the case. I feel like it would be cool to make it so you can scroll through different stats on the oled, and maybe even have a wifi choosing menu and led control menu. This complicates my design a lot, so I'm expecting my project to take a little more time than originally expected.

  I spent maybe an hour making the basic case, and then maybe another hour and a half adding functions like underglow, wiring paths, splitting parts, screw holes, etc.Again,ran into lots of blender problems. I love and hate formware at the same time. Oh, I also found a kind of cool but also kind of dorky name for it. "CardPulse". Its pretty freaking dorky lol. I also spent a lot of time redoing stuff and finding dimensions for some of the parts I don't have 3d models for. I also spent a ton of time fixing this one face of the lid, the auto repair tools didn't work so I had to manually do it, I spent like an hour fixing it.

<img width="1470" height="923" alt="Screenshot 2026-09-04 at 7 14 23 PM" src="https://github.com/user-attachments/assets/d4efaf37-b308-4847-95d1-449f3112eb9b" />

**Total time spent: 4 hours**

## September 4th: Added switch

Realized I didn't have an actual power switch. Added a KCD01, took forever for some reason. Spent maybe 10 minutes finding stl, and an hour - an hour and a half adding the part and hole for it to fit into the case, mainly because face where I added the switch hole get all messed up and I had to spend maybe 40 minutes repairing it. I also changed my mind a bumch about its placement, and blender crashed because I tried perform a weird boolean and I had to restart. I hate blender. I need to learn fusion.

<img width="699" height="561" alt="Screenshot 2026-09-04 at 7 47 01 PM" src="https://github.com/user-attachments/assets/0bd0267e-fd33-406f-aee6-b28d1963930e" />

**Total time spent: 1.5 hours**

## September 4th: Imported everything into bambu studio

I'm hoping that the cadding part of the project is done, I imported everything into bambu studio and arranged everything, set infills and wall loops, added strength modifiers, rotated some stuff, and filament mapped everything. Now I can hopefully start working on the code.

**Total time spent: 40 Minutes**
