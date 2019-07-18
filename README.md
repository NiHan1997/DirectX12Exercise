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
	