# Direct3D 12 龙书习题解答

# Chapter 6
* 01 Chapter 6 BoxApp ：在原书的基础上，使用了Shader Model 5.1 语法，将HLSL中的常量数据封装为结构体。
* 02 Chapter 6 Exercise_2 : 这道题需要重写d3dUtil.h中的相关方法，因此复制了一份Common文件在项目中，此题的难点  
&nbsp;&nbsp;&nbsp;&nbsp;在于创建两个顶点缓冲区，一个负责位置信息，另一个负责颜色信息，因此需要对原先一个缓冲区的结构进行修改,  
&nbsp;&nbsp;&nbsp;&nbsp;相关代码可以参考d3dUtil.h，内部给出了详细注释。
* 03 Chapter 6 Exercise_3 ：此题主要是利用多个PSO绘制不同的几何体，并且根据不同形态的几何体构建对应的索引。
* 04 Chapter 6 Exercise_7 ： 此题与上一题类似，都是绘制多个几何体，每个物体对应各自的顶点和索引。
* 05 Chapter 6 Exercise_12 ：设置视口可以参考龙书4.3.9，清除渲染目标中函数提供了相关参数，在这里还将颜色使用  
&nbsp;&nbsp;&nbsp;&nbsp;XMCOLOR进行压缩，但是需要注意输入装配的Format，颜色渐变效果这里使用了正弦函数。

# Chapter 7
* 06 Chapter 7 ShapesApp : 原书的基础上还原，使用了Shader Model 5.1 语法，将HLSL中的常量数据封装为结构体。
* 07 Chapter 7 WavesApp : 原书的基础上还原，波浪方程是此书最大的难点之一。
* 08 Chapter 7 Exercise : 此章节练习题比较简单，将两题合并，通过IO加载模型数据，将世界矩阵修改为16个根常量  
&nbsp;&nbsp;&nbsp;&nbsp;首先修改根签名，然后重构描述符堆相关代码，最后绑定资源也需要更换形式，在HLSL中还需要重构世界矩阵。

# Chapter 8
* 09 Chapter 8 LightWavesApp : 在上一章的基础上添加了基础光照，在像素着色器进行关照计算，顶点着色器计算  
&nbsp;&nbsp;&nbsp;&nbsp;光照会造成边缘锯齿。
* 10 Chapter 8 Exercise_1 : 此题在更新MainPassCB的函数中使用正弦函数让光照的R分量随时间变换即可，同时修改  
&nbsp;&nbsp;&nbsp;&nbsp;清理后台缓冲区时的默认颜色，让场景更丰富。
* 11 Chapter 8 Exercise_3 ：此题使用了三点光源(3个平行光，不是点光源)，后序的代码都是在此光照上的实现。
* 12 Chapter 8 Exercise_4 ：为了让点光源和聚光灯产生对比，在此将左边柱子旁边放置点光源，右边柱子旁边放置  
&nbsp;&nbsp;&nbsp;&nbsp;聚光灯，并去掉平行光，修改着色器相关光源数量，即可很明显观察出两种光源。
* 13 Chapter 8 Exercise_6 : 卡通着色就是要显出突变和高光，按照书上的方法，可以得到一个简单的卡通着色，但是  
&nbsp;&nbsp;&nbsp;&nbsp;效果确实不佳，以后会配合纹理和描边，能够实现丰富的卡通着色。

# Chapter 9
* 14 Chapter 9 CrateApp : 原书基础上还原，添加了纹理的箱子确实会好看很多。
* 15 Chapter 9 LandWavesApp : 原书基础上还原，添加了纹理，程序逐渐开始有趣，陆地海洋也开始漂亮了。
* 16 Chapter 9 Exercise_3 : 此题使用了两张纹理，这里初学可能会有一个误区，就是一个物体只能绑定一个材质，  
&nbsp;&nbsp;&nbsp;&nbsp;一个材质只有一张纹理，实质上在这里材质只是有一个纹理在描述符堆中索引，并没有绑定材质，最终着色器接收  
&nbsp;&nbsp;&nbsp;&nbsp;纹理还是在IA阶段。此题还有一个难点就是纹理旋转，二维旋转矩阵容易求出，就是在考虑旋转中心点时需要多下  
&nbsp;&nbsp;&nbsp;&nbsp;一点心思，具体可以参考此题的Shader/Default.hlsl，在顶点着色器中实现纹理旋转。
* 17 Chapter 9 Exercise_6 ：此题就是常规操作，难度不如上一题，当做复习练手即可。

# Chapter 10
* 18 Chapter 10 LightWavesBlendApp : 在纹理的基础上有实现了混合效果，本章习题也没有什么特别的，实现案例即可。

# Chapter 11
* 19 Chapter 11 StencilApp : 程序从这一章开始就变得有难度了，这里只是书上内容的还原，深度/模板测试相关的  
&nbsp;&nbsp;&nbsp;&nbsp;许多细节还是需要仔细思考的，否则很可能得不到正确的结果。
* 20 Chapter 11 Exercise_7 ：此题个人感觉放在上一章更合适，这里需要使用texconv命令行工具将纹理转化为.DDS  
&nbsp;&nbsp;&nbsp;&nbsp;格式，在这里我使用了一个闪电纹理的索引，每一帧绘制闪电的时候就重新指定一张纹理，并更新索引，这样就  
&nbsp;&nbsp;&nbsp;&nbsp;形成了画面的运动，在这里并没有使用模板缓冲区，因为透明度混合本来就是多重混合。
* 21 Chapter 11 Exercise_8 ：此题个人认为本章最难习题，因为模板缓冲区的值我们并不能直接读取。在这里基于屏幕  
&nbsp;&nbsp;&nbsp;&nbsp;后处理实现。首先我们需要在着色器中新建一个常量缓冲区，其中存放我们OM设置的模板参考值；然后我们绘制  
&nbsp;&nbsp;&nbsp;&nbsp;几何体的时候，让所有物体都能通过模板测试，并且将模板值+1；随后我们基于屏幕后处理，将模板参考值从1-5  
&nbsp;&nbsp;&nbsp;&nbsp;依次设置，每次只能绘制出对应参考值的像素；同时我们把这个参考值传递给存放模板参考值的常量缓冲区，就  
&nbsp;&nbsp;&nbsp;&nbsp;可以根据模板参考值的不同绘制出不同的颜色。这道题逻辑比较绕，不容易说清楚，直接看代码会更好。
* 22 Chapter 11 Exercise_9 : 此题根据题目提示完全可以自写，只是设置一下混合状态、关闭深度测试，这种策略观察  
&nbsp;&nbsp;&nbsp;&nbsp;over draw的效果比上一题更加明显。
* 23 Chapter 11 Exercise_10 : 这里可以当做模板缓冲区使用方法的一次巩固，类似的这里还将阴影也反射到了镜子中。  

# Chapter 12
* 24 Chapter 12 TreeBillboardApp : 使用几何着色器实现广告牌效果，几何着色器最大的特点就是可以拓展和销毁顶点，  
&nbsp;&nbsp;&nbsp;&nbsp;在本章习题中几何着色器的使用将会十分有趣。
* 25 Chapter 12 Exercise_1 : 此题是集合着色器的简单应用，用线条带绘制一个圆形，每一个线条带有两个顶点，几何  
&nbsp;&nbsp;&nbsp;&nbsp;着色器输出4个顶点，前2个保持不变，后2个在y轴上进行偏移，即可绘制出一个线条带圆柱。
* 26 Chapter 12 Exercise_2 ：此题一次细分比较简单，但是二次细分的逻辑比较绕，需要多下点心思。
* 27 Chapter 12 Exercise_3 ：此题爆炸效果实现比较简单，让顶点延表面法线扩张即可。
* 28 Chapter 12 Exercise_4 ：在这里顶点还是用三角形组合，但是在几何着色器中让三角形的两个顶点重合，因此看起  
&nbsp;&nbsp;&nbsp;&nbsp;来就是线段。本质上还是几何着色器拓展顶点。
* 29 Chapter 12 Exercise_5 ：这道题的原理和上一题是一样的，只是输出顶点的位置和数量不同，在这里以三角形的重心  
&nbsp;&nbsp;&nbsp;&nbsp;坐标输出表面法线。

# Chapter 13
* 30 Chapter 13 VectorAddApp : 计算着色器的简单应用之一，这里可以发挥想象，观察计算结果。 
* 31 Chapter 13 BlurApp :利用计算着色器实现高斯模糊，这里其实严格意义上不能叫做渲染到纹理，叫读取后备缓冲  
&nbsp;&nbsp;&nbsp;&nbsp;更加合适，类似于Unity中的GrabPass，真正意义上的渲染到纹理在后面会加以实现。
* 32 Chapter 13 Exercise_1 : 此题计算向量的长度，类似于此书的案例。
* 33 Chapter 13 Exercise_4 ：双边模糊也叫做保边模糊，最大的特点就是边缘会产生奇特的效果。在这里通过计算像素的  
&nbsp;&nbsp;&nbsp;&nbsp;平均灰度值，通过比较两个像素的灰度值差异再决定是否模糊，特别是边缘处，效果更佳明显，也因此双边模糊  
&nbsp;&nbsp;&nbsp;&nbsp;通常用于滤镜，高斯模糊是直接一团模糊，双边模糊要考虑两者的差异才决定是否模糊，这个差异的算法可以有  
&nbsp;&nbsp;&nbsp;&nbsp;许多，后序会使用到深度和法线差异，在这里为了简化就直接计算灰度值，但是模糊次数过多导致有噪点产生。
* 34 Chapter 13 Exercise_5 ：这里波浪相关算法确实看不懂，抄了一遍，以后看懂了再说，这个版本Bug也很明显。
* 35 Chapter 13 Exercise_6 ： 边缘检测Sobel算子是经典算法，在这里大体和原书一样，在计算着色器中使用了共享内存  
&nbsp;&nbsp;&nbsp;&nbsp;对程序进行进行优化，在处理边界和填充共享内存时需要格外小心。
* 36 Chapter 13 GuassBlur_RenderTexture ：这个程序属于额外练手，结合了渲染到纹理、屏幕后处理实现高斯模糊。

# Chapter 14
* 37 Chapter 14 BasicTessellation : 曲面细分着色器的初步使用，在原书案例上还原，虽然短期不会使用曲面细分技术，  
&nbsp;&nbsp;&nbsp;&nbsp;但是总体来讲3A游戏还是用的很多，还是掌握最佳。
* 38 Chapter 14 BezierPatch ：贝塞尔曲线和贝塞尔曲面可以说是图形学的精髓之一，当然数学原理实现起来也比较困难，  
&nbsp;&nbsp;&nbsp;&nbsp;但是效果确实非常好的。
* 39 Chapter 14 Exercise_1 ：三角形面片曲面细分和四边形曲面细分类似，只是控制点、面片类型有所差异，在处理线性  
&nbsp;&nbsp;&nbsp;&nbsp;插值也和四边形双线性插值有区别。
* 40 Chapter 14 Exercise_2 ：球体的曲面细分在曲面细分着色器中更加方便，由于我们细分的是三角形，因此在坐标对应  
&nbsp;&nbsp;&nbsp;&nbsp;时需要多留一个心思，否则很可能出现位置错误或者绕序错误。
* 41 Chapter 14 Exercise_7 ：3x3个控制点的二次贝塞尔曲面和4x4个控制点三次贝塞尔曲面类似，但是处理起来更简单。
* 42 Chapter 14 Exercise_8 ：在贝塞尔曲面上添加光照效果，需要顶点法线信息，顶点法线需要通过两次求偏导数获取  
&nbsp;&nbsp;&nbsp;&nbsp;两条切线，然后叉积即可得到法线；纹理采样的uv坐标在域着色器中已经提供，传递给像素着色器即可。
* 43 Chapter 14 Exercise_9 ：三角形贝塞尔曲面需要10个控制点，这里的原理可以参考项目中的 Bézier_triangle.pdf， 
&nbsp;&nbsp;&nbsp;&nbsp;最难理解的地方就是利用重心坐标公式求出对应的控制点位置，这也是贝塞尔曲线/曲面的魅力。