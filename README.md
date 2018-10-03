### Nintendont for Melee
See the [README](https://github.com/FIX94/Nintendont/blob/master/README.md) for standard Nintendont. It's just like that except:
* Native Control is locked to 'on'.
* It has codesets built-in, turn them on from the settings menu! (Can't guarantee compatibility with non-vanilla Melee ISOs or while using custom codesets through the 'Cheats' setting)
  * [OSReport](https://twitter.com/UnclePunch_/status/1017607009104023552): show error information if melee crashes. Take a photo and include it with your bug report!
  * [UCF 0.73](http://www.20xx.me/ucf.html): allow all controllers to dashback and shield drop equally. Includes OSReport and convenience codes like 'unlock everything', 'boot to css', etc.
  * [Arduino 1.01](https://twitter.com/ssbmhax/status/1046108551570214913): matches the hardware adapter solution to be used at tournaments where UCF cannot be used. Includes OSReport and convenience codes like 'unlock everything', 'boot to css', etc. WORKS ONLY WITH NTSC 1.02
  * All built-in codesets can be viewed in [PatchMeleeCodes.h](kernel/PatchMeleeCodes.h).


### Kadano's Wii softmodding guide
I recommend using [Kadano's guide](https://docs.google.com/document/d/1iaPI7Mb5fCzsLLLuEeQuR9-BeR8AOwvHyU-FM8GKmEs) if you're new to all this. Many guides in the wild are out of date or weren't very good to begin with.

### Installation Summary:
1. Download `NintendontForMelee.zip` from the [latest release](https://github.com/jmlee337/Nintendont/releases/latest). Unzip to the root of your SD card such that `boot.dol`, `meta.xml`, and `icon.png` all end up under `/apps/NintendontForMelee`.
2. Copy your vanilla Melee ISO (as well as any special versions like [Training Mode](https://www.patreon.com/UnclePunch)) to `/games/`.
3. Combine with [Priiloader](http://wiibrew.org/wiki/Priiloader) and [Nintendont Forwarder for Priiloader](https://github.com/jmlee337/Nintendont/loader/loader.dol) to autoboot from power-on to Nintendont.
4. Turn on autoboot in Nintendont settings to autoboot all the way to Melee.
