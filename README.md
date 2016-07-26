# DSMayaTools

This is a Maya plug-in to which I intend to add any useful custom nodes or commands that I develop. Currently it only contains a single new command named __closestVertex__. This adds a command to Maya which allows you to obtain the closest vertex of face on a poly mesh to a given position in world space. I've used this in some skinning scripts.

There are binary builds for Maya 2013 / 15 / 16 included.

# Installation

At the moment the installation is really simple. Simply copy

> \install\\\<Maya Version\>\plug-ins\DSMayaTools.mll

to one of your Maya plugin folders. These are usually something like the paths below

> C:\Users\\\<User\>\Documents\maya\\\<Maya Version\>\prefs

> C:\Program Files\Autodesk\\\<Maya Version\>\bin\plug-ins

When you reload Maya load the plug-in by going to __Windows >> Settings / Preferences >> Plug-in Manager__ and ticking load next the the __DSMayaTools.mll__ entry.




