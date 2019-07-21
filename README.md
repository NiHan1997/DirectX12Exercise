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