<p align="center">
<img src=".github/artwork.png" alt="Logo" width=2031> 
</p>


<p align="center">
<a href="">Callgraph</a> | <a href="">Notepad</a> | <a href="">Dockable Sidebars</a> | <a href="">MultiShortcut</a> | <a href="">XNU Tools</a> | <a href="">Reimagined theme</a>
</p>

This plugin is not officially associated or affiliated with Vector35 in any capacity.


## Dockable Sidebars

Drag your sidebar widgets where you need them, so you aren't constantly switching between widgets.

<p align="center">
<img src=".github/eg_sidebars.png" alt="Sidebars Example" width=1920> 
</p>

Sidebars support all 4 positions (at the 4 corners of the screen,) and widgets
can be moved around.

When a sidebar item is drag-dropped anywhere but a sidebar, it'll open as a separate window and float on top :)

## MultiShortcut

![](.github/eg_multishortcut.gif)

Hitting the key bound for the "KSuite" action (default K) (or running it via Command Palette) will
open a submenu with relevant bound actions triggerable by the keys surrounding "K".

This allows chaining easily rememberable keybinds to perform actions with granularity.
(e.g. `K U I U` will generate a downward callgraph from the current function, without psuedocode,
but `K U I I` will generate one with psuedocode). I have used this heavily, they quickly become
muscle memory.

## Callgraph

Multishortcut: `k` -> `u`  
Menu: `Plugins` -> `Callgraph`

<p align="center">
<img src=".github/eg_callgraph.png" alt="Callgraph Example" width=1920> 
</p>

Supports generating callgraphs with HLIL included or solely with names

Can generate a graph `N` calls into a func, out of a func, a variable amount in both directions, or of the entire program.

Runs entirely backgrounded and uses exclusively BinaryNinja APIs for a seamless and snappy integration into the product.
It's very fast :)


### Re-Imagined Theme

<p align="center">
<img src=".github/eg_theme_demo.png" alt="Theme Example" width=1920> 
</p>

A custom layout has been designed from the ground up (using the color base from [catppuccin](https://github.com/catppuccin))
built for modernity and legibility.

`<note> The theme is subject to change fairly heavily before this project hits 100% completion </note>`


## Darwin Kernel Tooling 

### Type Helper

This is a set of UIActions (also included in the Multishortcut menu) that assist with the typing of 
interesting methods in Kexts (particularly UserClient external methods for now).

### Darwin Kernel Workflow

This module workflow runs a few routines:
* Removes PAC from LLIL upward
* Consolidates certain SIMD code so it no longer takes up 16 HLIL lines per instruction
* Properly transforms jumps to unknown locations to tailcalls

