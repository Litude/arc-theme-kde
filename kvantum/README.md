# Arc Kvantum Application Styles
Arc styles (Regular, Dark and Darker) for [Kvantum](https://github.com/tsujan/Kvantum/tree/master/Kvantum).

## Installing
Requires [Kvantum](https://github.com/tsujan/Kvantum/tree/master/Kvantum) to be installed. Add the theme folders to ```~/.config/Kvantum/```.

Configure System Settings->Application Style to use kvantum and use Kvantum Manager to select Arc, ArcDark or ArcDarker.

## Customization
The theme uses some additional macOS inspired arrows by default to provide more variety in the types of arrows used. To change these arrows to the pointier GTK Arc arrow styles, open the respective ```.kvconfig``` file and perform any or all of the following changes:

* For menu items, locate the ```[MenuItem]``` section and change ```indicator.element=menuitem``` to ```indicator.element=menuitem-sharp```.

* For tree views, locate the ```[TreeExpander]``` section and change ```indicator.element=tree``` to ```indicator.element=tree-sharp```.

* For indicator arrows, locate the ```[IndicatorArrow]``` section and change ```indicator.element=iarrow``` to ```indicator.element=arrow```.

## Acknowledgments
* Based on [KvArk/KvArkDark](https://github.com/tsujan/Kvantum/tree/master/Kvantum) by tsujan
* Also based on [Arc/ArcDark/ArcDarker](https://github.com/PapirusDevelopmentTeam/arc-kde/) by varlesh
