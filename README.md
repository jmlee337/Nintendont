### Nintendont for Melee tournaments
See the [README](https://github.com/FIX94/Nintendont/blob/master/README.md) for standard Nintendont. It's just like that except:
* Native Control is locked to 'on'.
* It has codesets built-in, turn them on from the settings menu! ('Cheats' can still be used simultaneously)
  * [OSReport](https://twitter.com/UnclePunch_/status/1017607009104023552): show error information if melee crashes. Take a photo and include it with your bug report!
  * [UCF 0.73](http://www.20xx.me/ucf.html) (+ OSReport): allow all controllers to dashback and shield drop equally. Includes convenience codes like 'unlock everything', 'boot to css', etc.
  * All built-in codesets can be viewed in [PatchMeleeCodes.h](kernel/PatchMeleeCodes.h).


### Kadano's Wii softmodding guide
I recommend using [Kadano's guide](https://docs.google.com/document/d/1iaPI7Mb5fCzsLLLuEeQuR9-BeR8AOwvHyU-FM8GKmEs) if you're new to all this. Many guides in the wild are out of date or weren't very good to begin with.

### Installation Summary:
1. Get the [loader.dol](loader/loader.dol?raw=true), rename it to boot.dol and put it in `/apps/Nintendont/` along with the files [meta.xml](nintendont/meta.xml?raw=true) and [icon.png](nintendont/icon.png?raw=true).
   * Or just grab `Nintendont.zip` from the [latest release](https://github.com/jmlee337/Nintendont/releases/latest).
2. Copy your vanilla Melee ISOs to `/games/`.
3. Combine with [Priiloader](http://wiibrew.org/wiki/Priiloader) and [Nintendont Forwarder for Priiloader](https://github.com/jmlee337/Nintendont/loader/loader.dol) to autoboot from power-on to Nintendont.
4. Turn on autoboot in Nintendont settings to autoboot all the way to Melee.
