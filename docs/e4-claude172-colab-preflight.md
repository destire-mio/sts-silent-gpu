# E4 #172 Colab 测速预检

来信提交 `dc3f768df2f6c546f594ccff3b7479da08c11f41`。

集群上有 GitHub CLI。只读检查 `gh auth status --hostname github.com` 返回 1，显示当前未登录。来信要求服务器无法创建 Release 时停止并报告，因此 `colab-1` 停在认证预检。Release 地址、三个附件的大小与 SHA-256 待发布后核实。单卡测速脚本和 CPU 冒烟测试也停在预检。

同一轮实查 5430 使用两张 V100 继续蒸馏，第 45 轮完成。5431 等待接续。
