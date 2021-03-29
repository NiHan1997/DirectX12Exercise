# Direct3D 12 龙书习题解答

# 更新说明
在 VS 2019 新版本可能会出现" error C2102: “&”要求左值 "的错误，这是 C++ 的新的安全机制，解决方法也很简单，只需要将右值保存下来即可，例如：

原来的代码：
```c++
mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());
```
新的代码：
```c++
auto currentBackBufferView = CurrentBackBufferView();
auto depthStencilView = DepthStencilView();
mCommandList->OMSetRenderTargets(1, &currentBackBufferView, true, &depthStencilView);
```
这是一个比较繁琐的过程，在这里只提供正常运行的方法。

# Chapter 6
* 01 Chapter 6 BoxApp ：在原书的基础上，使用了Shader Model 5.1 语法，将HLSL中的常量数据封装为结构体。
* 02 Chapter 6 Exercise_2 : 这道题需要重写d3dUtil.h中的相关方法，因此复制了一份Common文件在项目中，此题的难点在于创建两个顶点缓冲区，一个负责位置信息，另一个负责颜色信息，因此需要对原先一个缓冲区的结构进行修改，相关代码可以参考d3dUtil工具类，内部给出了详细注释。
* 03 Chapter 6 Exercise_3 ：此题主要是利用多个PSO绘制不同的几何体，并且根据不同形态的几何体构建对应的索引。
* 04 Chapter 6 Exercise_7 ： 此题与上一题类似，都是绘制多个几何体，每个物体对应各自的顶点和索引。
* 05 Chapter 6 Exercise_12 ：设置视口可以参考龙书4.3.9，清除渲染目标中函数提供了相关参数，在这里还将颜色使用XMCOLOR进行压缩，但是需要注意输入装配的Format，颜色渐变效果这里使用了正弦函数。

# Chapter 7
* 06 Chapter 7 ShapesApp : 原书的基础上还原，使用了Shader Model 5.1 语法，将HLSL中的常量数据封装为结构体。
* 07 Chapter 7 WavesApp : 原书的基础上还原，波浪方程是此书最大的难点之一。
* 08 Chapter 7 Exercise : 此章节练习题比较简单，将两题合并，通过IO加载模型数据，将世界矩阵修改为16个根常量首先修改根签名，然后重构描述符堆相关代码，最后绑定资源也需要更换形式，在HLSL中还需要重构世界矩阵。

# Chapter 8
* 09 Chapter 8 LightWavesApp : 在上一章的基础上添加了基础光照，在像素着色器进行关照计算，顶点着色器计算光照会造成边缘锯齿。
* 10 Chapter 8 Exercise_1 : 此题在更新MainPassCB的函数中使用正弦函数让光照的R分量随时间变换即可，同时修改清理后台缓冲区时的默认颜色，让场景更丰富。
* 11 Chapter 8 Exercise_3 ：此题使用了三点光源(3个平行光，不是点光源)，后序的代码都是在此光照上的实现。
* 12 Chapter 8 Exercise_4 ：为了让点光源和聚光灯产生对比，在此将左边柱子旁边放置点光源，右边柱子旁边放置聚光灯，并去掉平行光，修改着色器相关光源数量，即可很明显观察出两种光源。
* 13 Chapter 8 Exercise_6 : 卡通着色就是要显出突变和高光，按照书上的方法，可以得到一个简单的卡通着色，但是效果确实不佳，以后会配合纹理和描边，能够实现丰富的卡通着色。

# Chapter 9
* 14 Chapter 9 CrateApp : 原书基础上还原，添加了纹理的箱子确实会好看很多。
* 15 Chapter 9 LandWavesApp : 原书基础上还原，添加了纹理，程序逐渐开始有趣，陆地海洋也开始漂亮了。
* 16 Chapter 9 Exercise_3 : 此题使用了两张纹理，这里初学可能会有一个误区，就是一个物体只能绑定一个材质，一个材质只有一张纹理，实质上在这里材质只是有一个纹理在描述符堆中索引，并没有绑定材质，最终着色器接收纹理还是在IA阶段。此题还有一个难点就是纹理旋转，二维旋转矩阵容易求出，就是在考虑旋转中心点时需要多下一点心思，具体可以参考此题的Shader/Default.hlsl，在顶点着色器中实现纹理旋转。
* 17 Chapter 9 Exercise_6 ：此题就是常规操作，难度不如上一题，当做复习练手即可。

# Chapter 10
* 18 Chapter 10 LightWavesBlendApp : 在纹理的基础上有实现了混合效果，本章习题也没有什么特别的，实现案例即可。

# Chapter 11
* 19 Chapter 11 StencilApp : 程序从这一章开始就变得有难度了，这里只是书上内容的还原，深度/模板测试相关的许多细节还是需要仔细思考的，否则很可能得不到正确的结果。
* 20 Chapter 11 Exercise_7 ：此题个人感觉放在上一章更合适，这里需要使用texconv命令行工具将纹理转化为.DDS格式，
在这里我使用了一个闪电纹理的索引，每一帧绘制闪电的时候就重新指定一张纹理，并更新索引，
这样就形成了画面的运动，在这里并没有使用模板缓冲区，因为透明度混合本来就是多重混合。
* 21 Chapter 11 Exercise_8 ：此题个人认为本章最难习题，因为模板缓冲区的值我们并不能直接读取。在这里基于屏幕后处理实现。首先我们需要在着色器中新建一个常量缓冲区，其中存放我们OM设置的模板参考值；然后我们绘制几何体的时候，让所有物体都能通过模板测试，并且将模板值+1；随后我们基于屏幕后处理，将模板参考值从1-5依次设置，每次只能绘制出对应参考值的像素；同时我们把这个参考值传递给存放模板参考值的常量缓冲区，就可以根据模板参考值的不同绘制出不同的颜色。这道题逻辑比较绕，不容易说清楚，直接看代码会更好。
* 22 Chapter 11 Exercise_9 : 此题根据题目提示完全可以自写，只是设置一下混合状态、关闭深度测试，这种策略观察over draw的效果比上一题更加明显。
* 23 Chapter 11 Exercise_10 : 这里可以当做模板缓冲区使用方法的一次巩固，类似的这里还将阴影也反射到了镜子中。  

# Chapter 12
* 24 Chapter 12 TreeBillboardApp : 使用几何着色器实现广告牌效果，几何着色器最大的特点就是可以拓展和销毁顶点，在本章习题中几何着色器的使用将会十分有趣。
* 25 Chapter 12 Exercise_1 : 此题是集合着色器的简单应用，用线条带绘制一个圆形，每一个线条带有两个顶点，
几何着色器输出4个顶点，前2个保持不变，后2个在y轴上进行偏移，即可绘制出一个线条带圆柱。
* 26 Chapter 12 Exercise_2 ：此题一次细分比较简单，但是二次细分的逻辑比较绕，需要多下点心思。
* 27 Chapter 12 Exercise_3 ：此题爆炸效果实现比较简单，让顶点延表面法线扩张即可。
* 28 Chapter 12 Exercise_4 ：在这里顶点还是用三角形组合，但是在几何着色器中让三角形的两个顶点重合，因此看起来就是线段。本质上还是几何着色器拓展顶点。
* 29 Chapter 12 Exercise_5 ：这道题的原理和上一题是一样的，只是输出顶点的位置和数量不同，在这里以三角形的重心坐标输出表面法线。

# Chapter 13
* 30 Chapter 13 VectorAddApp : 计算着色器的简单应用之一，这里可以发挥想象，观察计算结果。 
* 31 Chapter 13 BlurApp :利用计算着色器实现高斯模糊，这里其实严格意义上不能叫做渲染到纹理，叫读取后备缓冲更加合适，类似于Unity中的GrabPass，真正意义上的渲染到纹理在后面会加以实现。
* 32 Chapter 13 Exercise_1 : 此题计算向量的长度，类似于此书的案例。
* 33 Chapter 13 Exercise_4 ：双边模糊也叫做保边模糊，最大的特点就是边缘会产生奇特的效果。在这里通过计算像素的平均灰度值，通过比较两个像素的灰度值差异再决定是否模糊，特别是边缘处，效果更佳明显，也因此双边模糊通常用于滤镜，高斯模糊是直接一团模糊，双边模糊要考虑两者的差异才决定是否模糊，这个差异的算法可以有许多，后序会使用到深度和法线差异，在这里为了简化就直接计算灰度值，但是模糊次数过多导致有噪点产生。
* 34 Chapter 13 Exercise_5 ：在计算着色器实现波浪效果性能会明显提升，但是要注意不能限制帧率，因为必需在每次绘制之前更新波浪。
* 35 Chapter 13 Exercise_6 ： 边缘检测Sobel算子是经典算法，在这里大体和原书一样，在计算着色器中使用了共享内存对程序进行进行优化，在处理边界和填充共享内存时需要格外小心。
* 36 Chapter 13 GuassBlur_RenderTexture ：这个程序属于额外练手，结合了渲染到纹理、屏幕后处理实现高斯模糊。

# Chapter 14
* 37 Chapter 14 BasicTessellation : 曲面细分着色器的初步使用，在原书案例上还原，虽然短期不会使用曲面细分技术，但是总体来讲3A游戏还是用的很多，还是掌握最佳。
* 38 Chapter 14 BezierPatch ：贝塞尔曲线和贝塞尔曲面可以说是图形学的精髓之一，当然数学原理实现起来也比较困难，但是效果确实非常好的。
* 39 Chapter 14 Exercise_1 ：三角形面片曲面细分和四边形曲面细分类似，只是控制点、面片类型有所差异，在处理线性插值也和四边形双线性插值有区别。
* 40 Chapter 14 Exercise_2 ：球体的曲面细分在曲面细分着色器中更加方便，由于我们细分的是三角形，因此在坐标对应时需要多留一个心思，否则很可能出现位置错误或者绕序错误。
* 41 Chapter 14 Exercise_7 ：3x3个控制点的二次贝塞尔曲面和4x4个控制点三次贝塞尔曲面类似，但是处理起来更简单。
* 42 Chapter 14 Exercise_8 ：在贝塞尔曲面上添加光照效果，需要顶点法线信息，顶点法线需要通过两次求偏导数获取两条切线，然后叉积即可得到法线；纹理采样的uv坐标在域着色器中已经提供，传递给像素着色器即可。
* 43 Chapter 14 Exercise_9 ：三角形贝塞尔曲面需要10个控制点，这里的原理可以参考项目中的 Bézier_triangle.pdf，最难理解的地方就是利用重心坐标公式求出对应的控制点位置，这也是贝塞尔曲线/曲面的魅力。

# Chapter 15
* 44 Chapter 15 CameraAndDynamicIndexing : 这里终于初步接触了摄像机，至于动态索引就更简单了，理解原理即可，本章并没有难度。
* 45 Chapter 15 Exercise_2 ：此题完全没有难度，参考Camera.cpp即可实现。

# Chapter 16
* 46 Chapter 16 InstancingAndCulling ：本章简述了实例化的原理和操作，实例化是非常实用的操作，在引擎中已经深刻体会到；在这里视椎体剔除可以明显提高FPS，而不用等到硬件剔除，是程序优化的实用方法。
* 47 Chapter 16 Exercise_1 : 在这里包围球更简单，为了不破坏代码框架因此复制了一份Common文件到项目中，重写了d3dUtil中SubmeshGeometry的定义，获取包围球的数据更加简单，都不用考虑AABB和OBB的相关问题。

# Chapter 17
* 48 Chapter 17 Picking : 拾取这个操作实现起来并不难，主要还是理解射线检测相关的原理以及坐标变换的过程和意义。
* 49 Chapter 17 Exercise_1 : 实现此题的操作很简单，将上题小小改动即可。但是需要学习的还是原理以及它们的数学表示，这才是本章需要掌握的问题。

# Chapter 18
* 50 Chapter 18 CubeMap : 立方体纹理的使用很广泛，一般用来制作天空盒或者环境映射纹理在此基础上可以实现很多效果。
* 51 Chapter 18 DynamicCubeMap : 动态立方体纹理虽然效果更佳好，但是性能消耗很大，因为每一帧每一个需要渲染动态立方体纹理的物体都需要多渲染6张纹理，在一般的程序中尽量还是使用静态立方体纹理，但是动态立方体纹理的渲染思路上，可以实现很多其他的渲染效果，这种渲染到纹理的方式在第13章已经大量使用，后序还有阴影贴图也是以此为基础实现的，需要重点掌握。
* 52 Chapter 18 Exercise_3 : 折射的效果实现也比较简单，这里使用的是空气 ：水 = 1.0 ：1.5，效果按照数学原理实现，但是真实性并不是特别高。
* 53 Chapter 18 CubeMapGS : 这里按照书上所说在几何着色器中绘制动态立方体纹理，这里重构代码的难度比较大，需要改写的地方很多，特别是集合着色器绘制的过程，已经PassCB的重构，还有相关描述符的改变，但是绘制的效果还是很好的，在几何着色器输出数据较少的情况下，此方法绘制动态立方体纹理效果比上面更好。

# Chapter 19
* 54 Chapter 19 NormalMap : 在这里使用了另一种策略计算法线，缺点就是稍微不那么直观，但是功能还是很明确。
* 55 Chapter 19 Exercise_4 : 这里将光照从世界空间改为了切线空间，涉及到的变量很多，虽然改法并不复杂，但是我们整体都是在世界空间计算的，因此为了简化省略了反射球相关立方体纹理采样。
* 56 Chapter 19 Exercise_5 : 这里模拟波浪翻滚，实质上就是讲两张法线以不同的方向运动，从而形成不同的法线和，在加上纹理的运动，在视觉效果上更像波浪，但是和波浪处于一个平面上就能马上看出“破绽”。

# Chapter 20
* 57 Chapter 20 ShadowMap : 有了阴影贴图之后，我们程序的光照看起来就更加真实了，在这里只是模拟的平行光，平行光计算阴影需要的是透视投影，实现起来也比较简单，相关细节也不复杂。最重要的还是理解ShadowMap的原理，以及阴影边界处理的混合。
* 58 Chapter 20 Exercise_3 : 在这里使用了聚光灯，聚光灯的阴影需要使用图示投影矩阵，但是透视投影矩阵在偏移量上很容易过度偏移，
在这里效果总体来说还不错，过程也不复杂，就是讲原先的正交投影该为透视投影并切换光源即可。
* 59 Chapter 20 Exercise_11 : 点光源阴影的实现就要复杂一些，实现的过程需要结合Chapter 18 CubeMapGS思考，只是将渲染图像改成了深度写入，
特别是在坐标变换上，点光源是最复杂的，性能消耗也是最大的。

# Chapter 21
* 60 Chapter 21 SSAO : 屏幕空间实时环境光遮蔽这个案例算是将本书知识综合使用的一个案例，这个案例涉及到的知识最多，应该重点掌握。
其中，渲染到纹理、双边模糊、SSAO的计算在这里的使用方法很值得学习，唯一的不足就是没有使用计算着色器进行模糊运算，在习题中会有对应的练习。
* 61 Chapter 21 Exercise_2 : 在这里高斯模糊的结果显而易见没有双边模糊好，高斯模糊没有模糊条件，在此范围的像素都会参与模糊过程，这样做会让图线显得更加柔和，噪点虽然减少，但是边界却不明显，双边模糊很好的保留了纹理的边界，因此在滤镜中通常会使用双边模糊；在这里的SSAO也是，为了确定环境光遮蔽的具体边界，使用双边模糊能更好地模拟墙角偏暗效果，同时通过模糊次数控制图像不至于太尖锐。
* 62 Chapter 21 Exercise_3 : 在计算着色器中实现模糊过程并不复杂，相关的过程可以参考第13章，只是在纹理采样方面需要多下一些心思，在这里为了方便采样，将SSAO纹理的分辨率设置和后台缓冲区相同，这样做使SSAO纹理、法线纹理和深度纹理采样索引相同，3种纹理的采样索引相同，实现效果也更佳。
* 63 Chapter 21 Exercise_4 : 自相交是很严重的后果，效果完全是错误的，这个在以后类似的程序中也需要注意。

# Chapter 22
* 64 Chapter 22 QuaternionDemo : 在这里只是四元数的一个简单理解，四元数用的最多的还是处理旋转问题，在下一章角色动画中将会大量使用旋转，届时才能真正看到四元数的魅力。在这里还是以理解原理为主，毕竟四元数是3D数学中最复杂的一个，也是极其重要的知识。

# Chapter 23
* 65 Chapter 23 SkinnedMesh : 本章是角色动画基础理论将解，在这里明显可以看到坐标变换和旋转相关操作的重要性，本书使用的模型是.m3d格式，并不是一个通用格式的模型。角色动画也是图形学的一大分支，这里只是讲解了一点皮毛，而且还不通用。建议学习FBX SDK，FBX SDK专门解析.fbx格式的模型，相关API也不很复杂，当然现代商业游戏引擎为了追求极致的效率，往往不会直接使用.fbx文件，引擎都会将.fbx文件转换为引擎专用文件，例如Unity引擎就是转化为.prefab文件，我们学习过程中可以不用考虑如此复杂，在个人项目中FBX SDK足矣。

# 后序
* 龙书作为计算机图形学的开端是很不错的，但是学完此书之后，还有一些细节需要大家留意和继续学习。在这里补充一部分想到的龙书没有的知识：
* 1.描边算法，主要是是透明物体描边。
* 2.卡通着色。
* 3.多线程。
* 4.四叉树、八叉树、KD树。
* 5.阴影算法过时，建议另找资料学习。
* 在这里还推荐微软官方的DirectX-Graphics-Samples，这是一个很好的学习库，里面有很多实时更新的知识，例如多线程、HDR、光线追踪等技术，
是很好的Direct3D学习资料。至于之后的学习，网上已经有海量的资料库，在这里不再赘述。
