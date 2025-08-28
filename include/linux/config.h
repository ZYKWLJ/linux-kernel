#ifndef _CONFIG_H
#define _CONFIG_H

/*
 * 以下注释说明根设备（root device，即系统启动后作为根文件系统的存储设备）相关配置逻辑
 * 过去根设备是硬编码（写死在代码里）的，现在不再这样处理。
 * 若要修改默认的根设备，可去修改 boot/bootsect.s 文件里 “ROOT_DEV = XXX” 这一行的内容
 */
/*
 * The root-device is no longer hard-coded. You can change the default
 * root-device by changing the line ROOT_DEV = XXX in boot/bootsect.s
 */

/*
 * 下面这段用于配置键盘布局，根据实际使用的键盘类型取消对应宏的注释：
 * KBD_FINNISH 对应芬兰语键盘
 * KBD_US 对应美式键盘
 * KBD_GR 对应德语键盘
 * KBD_FR 对应法语键盘
 */
/*
 * define your keyboard here -
 * KBD_FINNISH for Finnish keyboards
 * KBD_US for US-type
 * KBD_GR for German keyboards
 * KBD_FR for Frech keyboard
 */
#define KBD_US           // 启用美式键盘配置，若用其他键盘，注释掉这行并取消对应宏的注释
/*#define KBD_GR */      // 德语键盘配置，默认注释
/*#define KBD_FR */      // 法语键盘配置，默认注释
/*#define KBD_FINNISH */ // 芬兰语键盘配置，默认注释

/*
 * 正常情况下，Linux 启动时能从 BIOS 获取硬盘的参数（磁头数、扇区数等）。
 * 但如果因为某些难以理解的原因（比如 BIOS 故障、兼容性问题等），获取参数失败，
 * 系统就无法正确识别硬盘，这时可以通过定义 HD_TYPE 宏，手动填入硬盘的必要信息来解决。
 *
 * HD_TYPE 宏的定义格式如下：
 * #define HD_TYPE { 磁头数, 每磁道扇区数, 柱面数, 写预补偿, 着陆区, 控制字节 }
 *
 * 要是有两块硬盘，多个硬盘的信息用逗号分隔：
 * #define HD_TYPE { 第一块硬盘的磁头数等参数 },{ 第二块硬盘的磁头数等参数 }
 */
/*
 * Normally, Linux can get the drive parameters from the BIOS at
 * startup, but if this for some unfathomable reason fails, you'd
 * be left stranded. For this case, you can define HD_TYPE, which
 * contains all necessary info on your harddisk.
 *
 * The HD_TYPE macro should look like this:
 *
 * #define HD_TYPE { head, sect, cyl, wpcom, lzone, ctl}
 *
 * In case of two harddisks, the info should be sepatated by
 * commas:
 *
 * #define HD_TYPE { h,s,c,wpcom,lz,ctl },{ h,s,c,wpcom,lz,ctl }
 */

/*
 * 这里给了一个示例，定义两块硬盘的参数，第一块是 type 2 类型，第二块是 type 3 类型：
 * #define HD_TYPE { 4,17,615,300,615,8 }, { 6,17,615,300,615,0 }
 *
 * 注意：对于磁头数（head）小于等于 8 的硬盘，控制字节（ctl）填 0；
 * 磁头数超过 8 的硬盘，控制字节填 8  。
 *
 * 如果你希望让 BIOS 自动识别硬盘类型，保持 HD_TYPE 未定义即可，这也是常规的做法。
 */
/*
 This is an example, two drives, first is type 2, second is type 3:

#define HD_TYPE { 4,17,615,300,615,8 }, { 6,17,615,300,615,0 }

 NOTE: ctl is 0 for all drives with heads<=8, and ctl=8 for drives
 with more than 8 heads.

 If you want the BIOS to tell what kind of drive you have, just
 leave HD_TYPE undefined. This is the normal thing to do.
*/

#endif