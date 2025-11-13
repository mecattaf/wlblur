Plans to bring in SwayFX features? · Issue #3 · dawsers/scroll


mecattaf
opened on May 4
Please fill out the following:
Description:
Hello dawsers, used hyprscroller in the past but had to return to swaywm. So excited to use scroll functionality again! Thank you so much for making your work public.
I have been using SwayFX for their cosmetic enhancements:
- Blur
- Anti-aliased rounded corners, borders, and titlebars
- Shadows 
- Dim unfocused windows
- Scratchpad treated as minimize
I would love to see those features in scroll as well. Are there any plans to integrate swayfx and their scenefx into scroll? This would be really amazing.
PS: Amazing work with the animations! Swayfx team has been working on them for quite some time now

Yours,
T

Activity

mecattaf
added 
enhancement
New feature or request
 on May 4
dawsers
dawsers commented on May 4
dawsers
on May 4
Owner
Hi, thanks for your comments!

I think SwayFX uses its own renderer and scene graph to support all the bling. I want to stay as close as possible to Sway as I can, keep compatibility and use their updates. I even plan to follow their release schedule, having a stable version which will have the same version number as Sway's.

When I started this project I wasn't even going to support animations, but I saw it would be easy to implement them, and did it because they provide some visual feedback. But my main interest is functionality over looks.

So for now, I have no plans to support anything related to a new renderer. Scratchpad changes could happen, maybe something similar to Hyprland's special workspaces, but Sway has support for "back and forth", so any workspace can be "special". I don't miss it much.


dawsers
dawsers commented 2 days ago
dawsers
2 days ago
Owner
In 8534de6 I modified the gles2 and vulkan renderers to support rounded borders and title bars, shadows that can be dynamic with blur and optionally dimming of inactive windows.

mecattaf
mecattaf commented 2 days ago
mecattaf
2 days ago
Author
This is major @dawsers !
Congrats on that. And thanks for the new features. Will update the package once new version is released. You MIGHT want to reset versioning to v0.1.0 since this is a major split off standard sway (foxed wlroots version etc)
I am also discontinuing the scrollfx project, will instead try my hand at importing blur from wlrfx into scroll's wlroots directly.

dawsers
dawsers commented 2 days ago
dawsers
2 days ago
Owner
Thank you. Yes, there will be a new version in a few days if I see there are no major bug reports. It will be 1.12, as from now on, we will be in sync with wlroots-git. I will also remove sway-scroll-satable and call it sway-scoll to avoid more confusion and follow better the packaging standard for the AUR. It is a good moment to do that because the dependencies change quite a bit now that we link wlroots statically.

WillPower3309
WillPower3309 commented 2 days ago
WillPower3309
2 days ago
This is major @dawsers ! Congrats on that. And thanks for the new features. Will update the package once new version is released. You MIGHT want to reset versioning to v0.1.0 since this is a major split off standard sway (foxed wlroots version etc) I am also discontinuing the scrollfx project, will instead try my hand at importing blur from wlrfx into scroll's wlroots directly.

Definitely understandable if you prefer to have in house fx - but if you're looking for minimal maintenance burden I'd encourage you to use scenefx - as someone who deals with rebases of both sway and wlroots I'd caution it's not a great time 😆

But if you're set on maintaining another fork - I'd recommend to copy the blur logic from the current scenefx release - in the main branch there were recently some pretty big changes we're still ironing out prior to a release. Expect blur to cause significant divergence from vanilla wlroots

dawsers
dawsers commented 2 days ago
dawsers
2 days ago
Owner
Definitely understandable if you prefer to have in house fx - but if you're looking for minimal maintenance burden I'd encourage you to use scenefx - as someone who deals with rebases of both sway and wlroots I'd caution it's not a great time 😆

Thank you, Will. I would have used scenefx, the problem is I also made a lot of modifications to wlr_scene for the content scaling and workspace scaling stuff, so I already had a "fork" of the scene in the code base. I wasn't even planning to add any FX, I just did it because at some point it made sense. When I decided to do it, it was easier to simply take the current state of wlroots, which was better connected to my scene changes.

Blur may happen, but it is not a priority right now, because it also requires more changes to the renderers. I had to modify the gles2 and vulkan renderers to add the new FX stuff, and I want everything to be stable and to know it better before I add changes to the rendering passes that will be needed for blur. wlroots is still making changes to the vulkan renderer, which will probably be the default one soon, so I'd rather wait to see how that ends. All the color stuff will require the vulkan renderer, so that is probably where things are moving right now.

There are still things to work on before adding blur :-)


---

SwayFX uses its own scene graph and renderer. Scroll uses wlroots's scene graph, but modified to support content and workspace scaling. After #3 there is an effort by the author of the issue report to merge both and create ScrollFX.

On my side, there are no current plans to implement any blur effects. For that, I would need to write a new renderer, and that would separate the code base even more from wlroots and sway. wlroots's main renderer is still GLES2, and there are plans to move to Vulkan as main target. Rewriting a GLES2 renderer when it will not be the main one soon feels like a wasted effort to me.

Implementing blur has a lot of side effects. It needs multiple rendering passes, affects damage tracking and clients would need a way to notify if they are applying some alpha effect or it wouldn't work well. It also makes performance worse, and it is very prone to bugs because there is no Wayland standard for clients. There are some Wayland extensions coming up to fix some of the issues, but things are not standard enough for me to go that route at this time.

---





