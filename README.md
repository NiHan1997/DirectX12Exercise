# Direct3D 12 龙书习题解答

# Chapter 6
* 01 Chapter 6 BoxApp ：在原书的基础上，使用了Shader Model 5.1 语法，将HLSL中的常量数据封装为结构体。
* 02 Chapter 6 Exercise_2 : 这道题需要重写d3dUtil.h中的相关方法，因此复制了一份Common文件在项目中，此题的难点  
&nbsp;&nbsp;&nbsp;&nbsp;在于创建两个顶点缓冲区，一个负责位置信息，另一个负责颜色信息，因此需要对原先一个缓冲区的结构进行修改,  
&nbsp;&nbsp;&nbsp;&nbsp;相关代码可以参考d3dUtil.h，内部给出了详细注释。
* 03 Chapter 6 Exercise_3 ：此题主要是利用多个PSO绘制不同的几何体，并且根据不同形态的几何体构建对应的索引。
	