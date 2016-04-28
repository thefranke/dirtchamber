Introduction
============

The Dirtchamber, inspired by the homonymous Prodigy album, is a mixed reality (MR) testing environment for real-time global illumination algorithms. It evolved while I was writing papers to figure out if a global illumination algorithm is suitable (with adaptations) to be used as a relighting solution for an MR scenario. Two main products came out of this: Delta Light Propagation Volumes and Delta Voxel Cone Tracing.

![XYZRGB Dragon next to a 3D printed one](http://www.tobias-franke.eu/publications/franke15phd/franke15phd.jpg "XYZRGB Dragon next to a 3D printed one")

This suite features four samples:

- A Light Propagation Volume renderer sample
- A Delta Light Propagation Volume MR renderer
- A Voxel Cone Tracing renderer sample
- A Delta Voxel Cone Tracing MR renderer

Both, MR and non-MR samples are rendered with a deferred renderer and a postprocessing pipeline which includes FXAA, SSAO, Depth-of-Field, crepuscular Rays, Bloom, a CRT monitor effect, TV grain, exposure adaptation and tonemapping. The model loader uses [Assimp](http://assimp.sourceforge.net/), which means you should be able to load a good range of different model formats. The deferred shader uses a physically-based BRDF model with GGX as its normal distribution function. Both MR samples as well as both non-MR renderers share a common file and switch between the volumetric GI method with a define.

The Dirtchamber uses Dune, a Direct3D helper library which includes many classes that simply wrap and manage the absurd amount of pointers and structs needed to upload stuff to the GPU. Hopefully, you'll find something useful in there too.

Tracking is realized with OpenCV and textures representing Kinect cameras or simple webcams. If you have access to neither, you can still use the tracking code with still images, since the tracking code only cares for cached textures which contain an image without regard for the source. This functionality is also useful to exchange scenes with other researchers to compare different rendering solutions for mixed reality.

The Delta Radiance Field
========================

The Dirtchamber is the product of 4 publications:

- **Near-field illumination for mixed reality with delta radiance fields**, ACM SIGGRAPH 2013 Posters
- **Delta Light Propagation Volumes for Mixed Reality**, 2013 IEEE International Symposium on Mixed and Augmented Reality (ISMAR)
- **Relighting of Arbitrary Rough Surfaces**, ACM SIGGRAPH 2014 Posters
- **Delta Voxel Cone Tracing**, 2014 IEEE International Symposium on Mixed and Augmented Reality (ISMAR)

You can download preprints at http://tobias-franke.eu/?pub.

The initial poster, presented at SIGGRAPH 2013, pitched the idea of the Delta Radiance Field (DRF). Up to this point, most if not all real-time relighting solutions were based on a highly influential paper by Paul Debevec which introduced a method called **Differential Rendering**. A synthetic copy of a real scene is rendered into one buffer \\( \\mu \\). The same scene is rendered again, but this time with an additional augmenting object \\( \\rho \\). By subtracting the former from the latter, one is left with the differential which can be added on top of a background image.

The intriguing aspect of Differential Rendering is its simplicity. However, when dealing with GI solutions, the cost of rendering the scene twice will sooner rather than later introduce a performance bottleneck. Another issue with this approach is inherent in its screen-space nature: volumetric information along the a light path between real and synthetic particles is lost.

To combat the latter problem, volumes instead of two screen-space renderings can be used to identify exchange between real and synthetic particles in mid air. However, calculating the differential of two volumes introduces even higher costs than Differential Rendering ever had. In this sense, Differential Radiance Fields aren't feasible. Instead, we can figure out which portion of light is left over to simulate inside a volume by calculating the difference between Reflective Shadow Maps.

It is this difference which, when used within a GI solution, produces a radiance field \\( L_\\Delta \\) representing the *delta* between the synthetic and real space.

$$
L_\\Delta = L_\\rho - L_\\mu = \\sum_{i=0}^\\infty \\mathbf{T}_{\\rho}^{i} L_e - \\sum_{i=0}^\\infty \\mathbf{T}_{\\mu}^{i} L_e = \\sum_{i=0}^\\infty \\mathbf{T}_{\\Delta_i} L_e
$$

Delta Light Propagation Volumes encode this DRF with the help of Light Propagation Volumes, whereas Delta Voxel Cone Tracing cone-traces volumes with the left-over injection of real bounces on synthetic surfaces, while tracing a separate structure encoding occlusions.

Compiling & Running
===================

Prerequisites
-------------

You will need to download the following tools:

- [CMake 2.8.12](http://www.cmake.org) (**REQUIRED**)
- [Visual Studio 2013 or higher](http://www.visualstudio.com/) (**REQUIRED**)

Furthermore, you will need to download the following dependencies:

- [Boost 1.56](http://www.boost.org) (**REQUIRED**)
- [Open Asset Import Library 3.1.1](http://assimp.sourceforge.net/) (**REQUIRED**)
- [DXUT for Direct3D 11](https://dxut.codeplex.com/) (**REQUIRED**)
- [Windows SDK for Windows 8](http://msdn.microsoft.com/en-us/windows/desktop/hh852363.aspx) (**REQUIRED**)
- [Kinect SDK](https://www.microsoft.com/en-us/kinectforwindows/)
- [OpenCV 2.4.9](http://opencv.org/)

To ensure binary compatibility, it is recommended to compile DXUT and Assimp yourself. Dune only uses header-only libraries from Boost, which therefore doesn't need to be built. OpenCV is provided as binary release for all major Microsoft compilers and works out of the box.

Here are the steps you should take:

- Install the Windows SDK for Windows 8
- Install or build OpenCV
- Download Boost and extract it.
- Download DXUT, extract it and open DXUT_2013.sln, then build the entire solution as both Debug and Release.
- Download or pull Assimp, run CMake, set CMAKE_INSTALL_PREFIX to a directory in your dev folder and create a build. Build the INSTALL target for both debug and release.
- Reach Nirvana.

Get the source
--------------

You can find the project at http://github.com/thefranke/dirtchamber.

    $ git clone https://github.com/thefranke/dirtchamber.git

Please post any issues to the [bugtracker on GitHub](https://github.com/thefranke/dirtchamber/issues).

Running CMake
-------------

Open the CMake GUI and point the source to the root directory of the Dirtchamber. Select your Visual Studio distribution with either 32 or 64 bit. Create a separate directory somewhere else and point "Where to build the binaries" to that directory, then click on **Configure**.

CMake should be able to identify all libraries automatically except for DXUT, Boost and Assimp. You will need to point the following variables to your build of these libraries:

- **Assimp_INCLUDE_DIR**: the include directory of your Assimp INSTALL build.
- **Boost_INCLUDE_DIR**: the path to the extracted Boost library.
- **DXUT_SDK_PATH**: the path to the extracted DXUT zip file.

After configuration, click on generate and you should have a working solution in your build folder.

Running the samples
-------------------

The solution has either two or four projects. This depends on whether or not OpenCV and the Kinect SDK are available, because both of them are needed to run the mixed reality samples.

The first two projects (**lpv** and **vct**) are sample implementations of regular Light Propagation Volumes and Voxel Cone Tracing. Both projects can be run with command line arguments which can be either files or path patterns (something like C:/foo/\*.obj) to the geometry which should be loaded.

The other two projects (**dlpv** and **dvct**) are the mixed reality applications which implement Delta Light Propagation Volumes and Delta Voxel Cone Tracing respectively. Similarly to the first two projects, both executables will try to load all command line arguments as files or file patterns. The important bit is that the **last** parameter is the synthetic object, while all other parameters are assumed to be real reconstructed scene geometry.

Once running, you can manipulate rendering settings and the geometric attributes of a main light source. If you repeatedly need to access the same configuration with the same camera position and orientation, you can save these settings with by pressing **p** on your keyboard. Pressing **l** opens a dialog to open the settings again. You can find a sample configuration in **data/demo_gi.xml**.

Understanding the code
----------------------

First of all, you can read the documentation either by browsing the source code directly, or generate a formatted help by running [Doxygen](http://www.stack.nl/~dimitri/doxygen/) in the **doc** subfolder.

    $ cd doc
    $ doxygen

A precompiled version [is available online](http://tobias-franke.eu/projects/dirtchamber/doc/). Most of the code is located in the helper library called **Dune**. There are several classes which mostly wrap up D3D11 state code and manage resources. Some files have a suffix **tools(.h|.cpp)** which usually contain free functions either doing trivial things such as making sure a file path is absolute or compiling shaders, or they handle one of the Dune containers. You can trivially include everything important with the header dune.h.

Several classes such as dune::tracker can contain code necessary to set up a few things or implementation details which should not be visible to a class user. Similarly to the Boost library, the code in these cases is wrapped up in a subnamespace called **detail**. This makes it easier to collapse and hide functionality when browsing the code which isn't necessary to understand the rest.

All objects in Dune are explicitly managed using member functions **create(...)** and **destroy()**. Because of this the state of an object is deemed invalid before calling **create(...)** or after calling **destroy()**. Member functions generally do not guard against calls with an invalid state.

All four samples together share a lot of code, most of it being GUI and GI rendering related. There are three file pairs:

- **common_gui** containts callbacks for DXUT GUI code, most of the shared GUI code (settings for postprocessing etc.), simple functions to access GUI element values, synchronization code between GUI and Dune elements and functions to read and write all settings to XML files.
- **common_dxut** contains non-GUI related callbacks of DXUT (keyboard handling, resizing etc.).
- **common_renderer** contains two classes: **common_renderer** contains everything to set up a simple deferred renderer with a postprocessing pipeline, and the derived **rsm_renderer<T>** which adds a main light source for which an RSM is rendered.
- **pppipe** is the default postprocessing pipeline which is added on top of all four samples.

Shaders are explicitely managed from the outside. This means that each object which requires one or more shaders has a member set_shader_x() or simply set_shader() which receives precompiled shaders. Each application therefore has one centralized function load_shaders() which will load every shader necessary for the application, compile all of them and pass them to their respective objects. When reloading shaders, this function is called again and will reload everything. Slots at which an object can expect which Direct3D buffer is handled the same way.

Objects which feature shader parameters which may be manipulated from the outside come with a member function parameters() which returns the internal cbuffer.

When loading files, each object will make sure that the URI entered is always in absolute relation to the executable path on the disk, not on the execution path.

Debugging the code
------------------

Dune uses a four-class system to log during runtime. These are:

- **tclog**: Used to log some useful information.
- **tcout**: Used to log a warning which does not disrupt the flow of execution.
- **tcerr**: Used to log an error which does disrupt the flow of execution.
- **exception**: Thrown whenever the program cannot continue operating at this point. The main function will catch all remainders.

All logging is routed through a special **logbuf** class which, apart from displaying nice popups, will write each line to a log file. All samples write to **data/log.txt**.

Future Improvements
===================

Real-time relighting is still a largely unexplored area. Although many lessons can be learned from the "offline relighting community" (i.e. augmenting legacy photographs or movies with path-traced solutions), many issues cannot simply be transferred to the real-time domain. If you want to research in this area or contribute to this project, here are a few things on my list:

- **Postprocessing**: Especially smaller cameras like webcams or the Kinect have terrible image quality, exhibiting artifacts such as noise, general blurriness, reduced or quantized color spectrum, motion blur, lens distortion and more. Apart from missing radiometric registration, true MR immersion often suffers from the disparity in quality between the synthetic object and the background image. There is currently no standard process by which one can extract parameters for all kinds of artifacts of a camera and use them for postprocesses which will correct/degrade the rendered synthetic object to match the background image. The Dirtchamber is also missing several of these effects at the moment, for which I plan to write shaders. There are several papers discussing this topic:
    - *Simulating Low-Cost Cameras for Augmented Reality Compositing* [[Klein and Murray 2010]](http://www.robots.ox.ac.uk/~gk/publications.html)
    - *Perceptual Issues in Augmented Reality Revisited* [[Kruijff et al. 2010]](http://www.hydrosysonline.eu/media_files/ismar2010.pdf)
    - *Adaptive Camera-Based Color Mapping For Mixed-Reality Applications* [[Knecht et al. 2011]](http://www.cg.tuwien.ac.at/research/publications/2011/knecht-2011-CBCM/)
    - *Fast and Stable Color Balancing for Images and Augmented Reality* [[Oskam et al. 2012]](http://oskam.ch/research/interactive_color_balancing.pdf)
    - *Handling Motion-Blur in 3D Tracking and Rendering for Augmented Reality* [[Park et al. 2012]](http://cvlabwww.epfl.ch/~lepetit/papers/park_tvcg12.pdf)
    - *Color Distribution Transfer for Mixed-Reality Applications* [[Spleitz 2014]](http://www.cescg.org/CESCG-2014/papers/Spelitz-Color_Distribution_Transfer_for_Mixed-Reality_Applications.pdf)
- **Global Illumination**: Real-time GI in MR is closely tied to solutions presented in the general gaming environment. Because most MR applications rely on absolute flexibility and dynamic behavior, adapting algorithms might end up challenging. Apart from the volumetric methods implemented here or PRT-based solutions for rigid objects, tiled-based rendering can increase the amount of VPLs in Instant Radiosity solutions to suppress flickering. Furthermore, just like game artists usually tag objects as either static or dynamic, rigid objects in a real scene (such as large furniture) could rely on different GI paths than dynamic objects. Screen space methods haven't been properly investigated yet to enhance the image with small scale effects and local reflections.
- **Perceptual Rendering**: Since synthetic objects are exposed directly for comparison to the real environment, one might tend to go overboard with rendering the object as physically correct as possible at the expense of GPU time. Here several experiments would come in handy to see which effects and errors the human visual system will overlook more easily than others to cut down on shading costs.
- **Material Reconstruction**: Online material reconstruction, especially for non-diffuse parameters, is a big mystery. While there are several methods to faithfully reconstruct material parameters with complicated dome setups (the [MERL BRDF Database](http://www.merl.com/brdf/) for instance), quickly evaluating these from a single image in a couple of milliseconds remains an elusive goal. Having a robust depth and normal image of the real scene from a depth sensor (with proper filtering to close gaps) is currently missing in the code.
- **AR Rift**: Integrating stereo rendering into the Dirtchamber is a nice-to-have feature for [AR Rift setups](http://willsteptoe.com/post/66968953089/ar-rift-part-1), which will increase MR immersion immensely. However, one has to be careful to adjust for the cost of rendering the image twice.
- **Direct3D 12**: Handling volumetric data will probably benefit the most from a general code update to D3D12.
- **MR Authoring**: While this isn't technically a research topic, a tool in the spirit of [EnvyDepth](http://vcg.isti.cnr.it/Publications/2013/BCDCPS13/) or parts of a new system presented at SIGGRAPH 2014 [[Karsch et al. 2014]](http://kevinkarsch.com/?portfolio=automatic-scene-inference-for-3d-object-compositing) would tremendously help in setting up new scenes quickly.

License
=======

The entire source code of The Dirtchamber and Dune is licensed as MIT. If you have any specific questions about the license please let me know at tobias.franke@siggraph.org. See [LICENSE](http://github.com/thefranke/dirtchamber/blob/master/LICENSE) for more information.
