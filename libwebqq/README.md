libwebqq = 终极 webqq 协议实现
==

libwebqq 围绕 Boost.Asio 展开设计, 要了解 libwebqq 需要首先具有 Asio 的相关知识.


# Overview

libwebqq 设计为一个异步的 webqq 协议实现.
libwebqq 的用法非常简单, 构造一个 webqq 对象, 当 io_service::run() 下次执行的时候,  libwebqq 即开始启动登录过程.
如果需要设定和修改什么的话, 需要在下次 io_service::run() 执行前就做好,  如设定消息回调, 验证码回调等.

例如

```c++

boost::asio::io_service io_service;

webqq myqq(io_service, "your qqnum", "your qqpassword");

myqq.on_group_msg(&my_qq_message_callback);
myqq.on_verify_code(&my_qq_vcode_decoder);

io_service.run();

```

当程序流程走到 io_service.run() 的时候,  libwebqq 内部的循环即被带动起来了.
libwebqq 会自动处理登录, 掉线自动重新登录等过程. 用户只要在 消息回调里处理收到的消息即可.
如果QQ需要验证码, 则用户需要在 验证码回调里解码, 然后调用 webqq::feed_vc() 将解码的验证码喂过去.

> 注意, 出现验证码问题后, libwebqq 内部的循环即停止, 一直等待喂一个有效的验证码.


如果需要发送消息,  调用 webqq::send_group_message 即可. 那个 group 号码并不是 QQ 号码, 而是每次登录都会获得的一个随机的 id.
消息回调里的 id 即是.

libwebqq 设计为简单的使用, 并没有提供退出登录的直接 API . 因为只要webqq对象简单的退出作用域即可向其内部循环发起推出登录的操作!
webqq对象可以随时撤销, libwebqq 内部的循环并不依赖 webqq 对象, 当然这个对象的消失确实给内部循环发送了撤消信息.
于是收到撤销信息后, libwebqq 的内部循环就可以安全的退出了, 并释放一切占用的资源.

