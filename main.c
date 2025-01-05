//
//  main.c
//  dperf
//
//  Created by Nian on 2024/5/26.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <arpa/inet.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_bus_pci.h>

/* workaround to avoid conflicts between dpdk and lwip definitions */
#undef IP_DF
#undef IP_MF
#undef IP_RF
#undef IP_OFFMASK

#include <lwip/opt.h>
#include <lwip/init.h>
#include <lwip/pbuf.h>
#include <lwip/netif.h>
#include <lwip/etharp.h>
#include <lwip/tcpip.h>
#include <lwip/tcp.h>
#include <lwip/timeouts.h>
#include <lwip/prot/tcp.h>

#include <netif/ethernet.h>

#define MAX_PKT_BURST (32)
#define NUM_SLOT (256)

#define MEMPOOL_CACHE_SIZE (256)

#define PACKET_BUF_SIZE (1518)

#define HTTP_REQ_FORMAT         \
"GET %s HTTP/1.1\r\n"       \
"Connection: Keep-Alive\r\n"     \
"Host: %s\r\n"     \
"\r\n"

#define HTTP_RSP_FORMAT         \
"HTTP/1.1 200 OK\r\n"       \
"Content-Length: %lu\r\n"  \
"Content-Type: %s\r\n"  \
"Connection: keep-alive\r\n" \
"\r\n"                      \
"%.*s\n"

// MBUF_DATA_SIZE is same as dperf size
#define MBUF_DATA_SIZE      (1024 * 10)


/*
 以下代码用来随机生成一些测试数据
 */

/*
 身份证
 srand(time(NULL)); // 初始化随机数种子
 char id[19]; // 18位身份证号码加上结束符
 
 generate_id(id);
 */
// 地址码数组，可以根据实际需要增加或修改
const char* address_codes[] = {
    "110000", // 北京市
    "120000", // 天津市
    "130000", // 河北省
    // 更多地址码...
};

#define ADDRESS_COUNT (sizeof(address_codes) / sizeof(address_codes[0]))

// 计算校验码
char calculate_checksum(char *id) {
    int weights[] = {7, 9, 10, 5, 8, 4, 2, 1, 6, 3, 7, 9, 10, 5, 8, 4, 2};
    char checksum_chars[] = "10X98765432";
    int sum = 0;
    int i = 0;
    
    for (i = 0; i < 17; ++i) {
        sum += (id[i] - '0') * weights[i];
    }
    
    int mod = sum % 11;
    return checksum_chars[mod];
}

// 生成随机身份证号码
void generate_id(char *id) {
    int i = 0;
    
    // 随机选择一个地址码
    const char* address_code = address_codes[rand() % ADDRESS_COUNT];
    for (i = 0; i < 6; ++i) {
        id[i] = address_code[i];
    }
    
    // 生成随机出生日期
    int year = 1900 + abs(rand() % 101); // 假设1900-2000年之间的随机年份
    int month = 1 + abs(rand() % 12);
    int day = 1 + abs(rand() % 28); // 简化处理，假设每月都有28天
    
    snprintf(id + 6, 9, "%04d%02d%02d", year, month, day);
    
    // 生成随机顺序码
    for (i = 14; i < 17; ++i) {
        id[i] = '0' + rand() % 10;
    }
    
    // 计算并设置校验码
    id[17] = calculate_checksum(id);
    id[18] = '\0'; // 结束符
}


/*
 手机号
 srand(time(NULL)); // 初始化随机数种子
 char phone[12]; // 11位手机号码加上结束符
 
 generate_phone_number(phone);
 */

// 常见的中国大陆手机号码前缀
const char* phone_prefixes[] = {
    "130", "131", "132", "133", "134", "135", "136", "137", "138", "139", // 移动
    "150", "151", "152", "153", "155", "156", "157", "158", "159", // 联通
    "180", "181", "182", "183", "184", "185", "186", "187", "188", "189"  // 电信
    // 可以根据需要增加其他前缀
};

#define PREFIX_COUNT (sizeof(phone_prefixes) / sizeof(phone_prefixes[0]))

// 生成随机手机号码
void generate_phone_number(char *phone) {
    int i = 0;
    // 随机选择一个前缀
    const char* prefix = phone_prefixes[rand() % PREFIX_COUNT];
    for (i = 0; i < 3; ++i) {
        phone[i] = prefix[i];
    }
    
    // 生成剩余的8位数字
    for (i = 3; i < 11; ++i) {
        phone[i] = '0' + rand() % 10;
    }
    
    phone[11] = '\0'; // 结束符
}

/*
 车牌号
 srand(time(NULL)); // 初始化随机数种子
 char plate[10]; // 车牌号（1个汉字+1个字母+5个字母或数字+结束符）
 
 generate_license_plate(plate);
 
 printf("生成的车牌号: %s\n", plate);
 */
// 常见的省份简称
const char* provinces[] = {
    "京", "津", "沪", "渝", "冀", "豫", "云", "辽", "黑", "湘",
    "皖", "鲁", "新", "苏", "浙", "赣", "鄂", "桂", "甘", "晋",
    "蒙", "陕", "吉", "闽", "贵", "粤", "青", "藏", "川", "宁",
    "琼"
};

#define PROVINCE_COUNT (sizeof(provinces) / sizeof(provinces[0]))

// 生成随机车牌号
void generate_license_plate(char *plate) {
    int i = 0;
    // 随机选择一个省份简称
    const char* province = provinces[rand() % PROVINCE_COUNT];
    for (i = 0; i < 3; ++i) {
        plate[i] = province[i];
    }
    
    // 生成随机的地区代码（字母）
    plate[3] = 'A' + rand() % 26;
    
    // 生成剩余的5位字母或数字
    for (i = 4; i < 9; ++i) {
        int random_choice = rand() % 36;
        if (random_choice < 10) {
            plate[i] = '0' + random_choice; // 0-9
        } else {
            plate[i] = 'A' + (random_choice - 10); // A-Z
        }
    }
    
    plate[9] = '\0'; // 结束符
}

/*
 银行卡号
 srand(time(NULL)); // 初始化随机数种子
 char card[17]; // 16位银行卡号加上结束符
 
 generate_bank_card(card);
 
 printf("生成的银行卡号: %s\n", card);
 */

// 常见的发卡行标识代码（示例）
const char* bin_prefixes[] = {
    "622202", // 中国工商银行
    "622848", // 中国农业银行
    "622700", // 中国建设银行
    "622262", // 中国银行
    "622155", // 交通银行
    // 更多BIN前缀...
};

#define BIN_COUNT (sizeof(bin_prefixes) / sizeof(bin_prefixes[0]))

// 计算Luhn校验位
int calculate_luhn(const char *number) {
    int i = 0;
    int sum = 0;
    int len = 15;
    int parity = len % 2;
    
    for (i = 0; i < len; i++) {
        int digit = number[i] - '0';
        if (i % 2 == parity) {
            digit *= 2;
            if (digit > 9) {
                digit -= 9;
            }
        }
        sum += digit;
    }
    
    return (10 - (sum % 10)) % 10;
}

// 生成随机银行卡号
void generate_bank_card(char *card) {
    int i = 0;
    // 随机选择一个发卡行标识代码
    const char* bin = bin_prefixes[rand() % BIN_COUNT];
    for (i = 0; i < 6; ++i) {
        card[i] = bin[i];
    }
    
    // 生成随机的个人账号标识
    for (i = 6; i < 15; ++i) {
        card[i] = '0' + rand() % 10;
    }
    
    // 计算并设置校验位
    card[15] = '0' + calculate_luhn(card);
    card[16] = '\0'; // 结束符
}

/*
 工商号
 srand(time(NULL)); // 初始化随机数种子
 char reg[16]; // 15位工商注册号加上结束符
 
 generate_business_registration(reg);
 
 printf("生成的工商注册号: %s\n", reg);
 */
// 计算校验位
int calculate_checksum_business_registration(const char *number) {
    int weights[] = {1, 3, 9, 27, 19, 26, 16, 17, 20, 29, 25, 13, 8, 24};
    
    int i = 0;
    int sum = 0;
    
    for (i = 0; i < 14; i++) {
        sum += (number[i] - '0') * weights[i];
    }
    
    int checksum = 31 - (sum % 31);
    return checksum == 31 ? 0 : checksum;
}

// 生成随机工商注册号
void generate_business_registration(char *reg) {
    int i = 0;
    // 随机生成前14位数字
    for (i = 0; i < 14; ++i) {
        reg[i] = '0' + rand() % 10;
    }
    
    // 计算并设置校验位
    int checksum = calculate_checksum_business_registration(reg);
    reg[14] = '0' + checksum;
    reg[15] = '\0'; // 结束符
}


/*
 邮箱信息
 srand(time(NULL)); // 初始化随机数种子
 char email[50]; // 假设邮箱地址长度不超过50
 
 generate_email(email);
 
 printf("生成的邮箱地址: %s\n", email);
 */
// 常见的域名
const char* domains[] = {
    "gmail.com",
    "yahoo.com",
    "hotmail.com",
    "163.com",
    "qq.com",
    "sina.com"
};

#define DOMAIN_COUNT (sizeof(domains) / sizeof(domains[0]))

// 生成随机用户名
void generate_username(char *username, int length) {
    int i = 0;
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (i = 0; i < length; ++i) {
        username[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    username[length] = '\0'; // 结束符
}

// 生成随机邮箱地址
void generate_email(char *email) {
    char username[11]; // 假设用户名长度为10
    generate_username(username, 10);
    
    // 随机选择一个域名
    const char* domain = domains[rand() % DOMAIN_COUNT];
    
    // 组合用户名和域名生成邮箱地址
    snprintf(email, 50, "%s@%s", username, domain);
}



static struct rte_mempool *pktmbuf_pool = NULL;
static int tx_idx = 0;
static struct rte_mbuf *tx_mbufs[MAX_PKT_BURST] = { 0 };

static char *httpbuf;
static char *content;
static size_t httpdatalen;

static uint8_t random_url = 0;
static uint8_t response_type = 1;
/*
 1: 身份证
 2: 手机号
 4: 车牌号
 8: 银行卡号
 16: 工商注册号
 32: 邮箱地址
 */
static uint32_t sdd_data_type = 1|2|4|8|16|32;
static size_t max_content_len = 1024;
const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

static void tx_flush(void)
{
    int xmit = tx_idx, xmitted = 0;
    while (xmitted != xmit)
        xmitted += rte_eth_tx_burst(0 /* port id */, 0 /* queue id */, &tx_mbufs[xmitted], xmit - xmitted);
    tx_idx = 0;
}

static err_t low_level_output(struct netif *netif __attribute__((unused)), struct pbuf *p)
{
    char buf[PACKET_BUF_SIZE];
    void *bufptr, *largebuf = NULL;
    if (sizeof(buf) < p->tot_len) {
        largebuf = (char *) malloc(p->tot_len);
        assert(largebuf);
        bufptr = largebuf;
    } else
        bufptr = buf;
    
    pbuf_copy_partial(p, bufptr, p->tot_len, 0);
    
    assert((tx_mbufs[tx_idx] = rte_pktmbuf_alloc(pktmbuf_pool)) != NULL);
    assert(p->tot_len <= RTE_MBUF_DEFAULT_BUF_SIZE);
    rte_memcpy(rte_pktmbuf_mtod(tx_mbufs[tx_idx], void *), bufptr, p->tot_len);
    rte_pktmbuf_pkt_len(tx_mbufs[tx_idx]) = rte_pktmbuf_data_len(tx_mbufs[tx_idx]) = p->tot_len;
    if (++tx_idx == MAX_PKT_BURST)
        tx_flush();
    
    if (largebuf)
        free(largebuf);
    return ERR_OK;
}

static unsigned long io_stat[3] = { 0 };
static int num_stat = 0;

struct http_response {
    int state;
    long content_tot_len;
    long content_recvd;
    long cur;
    char buf[1UL << 16];
};

static err_t tcp_recv_handler(void *arg, struct tcp_pcb *tpcb,
                              struct pbuf *p, err_t err)
{
    int i = 0;
    if (err != ERR_OK)
        return err;
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }
    io_stat[1] += p->tot_len;
    if (!arg) { /* server mode */
        // 考虑到这里可能需要支持POST，PUT等情况的协议，不再进行协议验证，统一返回同样的内容
        // 后续如果需要需要，可以通过这里返回不同数据格式内容
        //
        //        char buf[4] = { 0 };
        //        pbuf_copy_partial(p, buf, 3, 0);
        //
        size_t content_len = 0;
        size_t buflen = 256 + max_content_len /* for http hdr */;
        
        uint32_t n = response_type;
        if (n == 0) {
            // 随机4选1模式
            n = (((uint64_t) random() * 4) / 0x80000000);
        }
        
        //以下方法现阶段不会超过10240，不想处理了，请注意，后续加的时候需要注意！！！
        
        if (n == 1) {
            /*
             {"ss": "11", "rr": "22"}
             */
            
            // [0] must {
            content[0] = '{';
            content_len += 1;
            
            if ((sdd_data_type & 1) && random() % 2) {
                char id[19]; // 18位身份证号码加上结束符
                generate_id(id);
                content_len += snprintf(content+content_len, 19 + 13, "\"cardNo\": \"%s\",", id);
            }
            
            if ((sdd_data_type & 2) && random() % 2) {
                char phone[12]; // 11位手机号码加上结束符
                generate_phone_number(phone);
                content_len += snprintf(content+content_len, 12 + 12, "\"phone\": \"%s\",", phone);
            }
            
            if ((sdd_data_type & 4) && random() % 2) {
                char plate[10]; // 车牌号（1个汉字+1个字母+5个字母或数字+结束符）
                generate_license_plate(plate);
                content_len += snprintf(content+content_len, 10 + 12, "\"plate\": \"%s\",", plate);
            }
            
            if ((sdd_data_type & 8) && random() % 2) {
                char card[17]; // 16位银行卡号加上结束符
                generate_bank_card(card);
                content_len += snprintf(content+content_len, 17 + 11, "\"card\": \"%s\",", card);
            }
            
            if ((sdd_data_type & 16) && random() % 2) {
                char reg[16]; // 15位工商注册号加上结束符
                generate_business_registration(reg);
                content_len += snprintf(content+content_len, 16 + 10, "\"reg\": \"%s\",", (reg));
            }
            
            if ((sdd_data_type & 32) && random() % 2) {
                char email[50]; // 假设邮箱地址长度不超过50
                generate_email(email);
                content_len += snprintf(content+content_len, 50 + 12, "\"email\": \"%s\",", email);
            }
            
            // left_size代表上文都执行完和要求的数据之间的差距
            size_t left_size = max_content_len - content_len;
            if (left_size > 13) {
                content_len += snprintf(content+content_len, 11, "\"other\": \"");
                memset(content+content_len, 'A', max_content_len-content_len);
                content[max_content_len-3] = '"';
                content[max_content_len-2] = '}';
                
                content_len = max_content_len -1;
            } else {
                if (content_len == 1) {
                    // json格式最小为{}单元
                    content_len = 2;
                }
                // 此处故意不闭合，请不要\0， rsp_format里已经用了.*s
                content[content_len-1] = '}';
            }
            
            memset(content + content_len, '\0', max_content_len - content_len);
            
            // json
            httpdatalen = snprintf(httpbuf, buflen, HTTP_RSP_FORMAT, content_len + 1, "application/json", (int) content_len, content);
        } else if (n == 2) {
            /*
             <?xml version="1.0" encoding="UTF-8"?>
             <note>
             <to>Tove</to>
             <from>Jani</from>
             <heading>Reminder</heading>
             <body>Don't forget me this weekend!</body>
             </note>
             */
            content_len = sprintf(content, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
            content_len += sprintf(content+content_len, "<xia>");
            
            // 身份证
            if ((sdd_data_type & 1) && random() % 2) {
                char id[19]; // 18位身份证号码加上结束符
                generate_id(id);
                content_len += snprintf(content+content_len, 19 + 17, "<cardNo>%s</cardNo>", id);
            }
            
            if ((sdd_data_type & 2) && random() % 2) {
                char phone[12]; // 11位手机号码加上结束符
                generate_phone_number(phone);
                content_len += snprintf(content+content_len, 12 + 15, "<phone>%s</phone>", phone);
            }
            
            if ((sdd_data_type & 4) && random() % 2) {
                char plate[10]; // 车牌号（1个汉字+1个字母+5个字母或数字+结束符）
                generate_license_plate(plate);
                content_len += snprintf(content+content_len, 10 + 15, "<plate>%s</plate>", plate);
            }
            
            if ((sdd_data_type & 8) && random() % 2) {
                char card[17]; // 16位银行卡号加上结束符
                generate_bank_card(card);
                content_len += snprintf(content+content_len, 17 + 13, "<card>%s</card>", card);
            }
            
            if ((sdd_data_type & 16) && random() % 2) {
                char reg[16]; // 15位工商注册号加上结束符
                generate_business_registration(reg);
                content_len += snprintf(content+content_len, 16 + 11, "<reg>%s</reg>", reg);
            }
            
            if ((sdd_data_type & 32) && random() % 2) {
                char email[50]; // 假设邮箱地址长度不超过50
                generate_email(email);
                content_len += snprintf(content+content_len, 50 + 15, "<email>%s</email>", email);
            }
            
            
            // left_size代表上文都执行完和要求的数据之间的差距
            size_t left_size = max_content_len - content_len;
            if (left_size > 15 + 6) {
                // <other></other>
                content_len += sprintf(content+content_len, "<other>");
                memset(content+content_len, 'A', max_content_len-content_len);
                (void) sprintf(content + max_content_len-8-6-1, "</other>");
                
                content_len = max_content_len - 6 -1;
            }
            
            content_len += sprintf(content + content_len, "</xia>");
            
            memset(content + content_len, '\0', max_content_len - content_len);
            
            // xml
            httpdatalen = snprintf(httpbuf, buflen, HTTP_RSP_FORMAT, content_len + 1, "application/xml", (int) content_len, content);
        } else if (n == 3) {
            /*
             1234567890
             12345
             123456
             */
            
            if ((sdd_data_type & 1) && random() % 2) {
                char id[19]; // 18位身份证号码加上结束符
                generate_id(id);
                content_len = snprintf(content, 1 + 19, "%s\n", id);
            }
            
            if ((sdd_data_type & 2) && random() % 2) {
                char phone[12]; // 11位手机号码加上结束符
                generate_phone_number(phone);
                content_len += snprintf(content + content_len, 1 + 12, "%s\n", phone);
            }
            
            if ((sdd_data_type & 4) && random() % 2) {
                char plate[10]; // 车牌号（1个汉字+1个字母+5个字母或数字+结束符）
                generate_license_plate(plate);
                content_len += snprintf(content + content_len, 1 + 10, "%s\n", plate);
            }
            
            if ((sdd_data_type & 8) && random() % 2) {
                char card[17]; // 16位银行卡号加上结束符
                generate_bank_card(card);
                content_len += snprintf(content + content_len, 1 + 17, "%s\n", card);
            }
            
            if ((sdd_data_type & 16) && random() % 2) {
                char reg[16]; // 15位工商注册号加上结束符
                generate_business_registration(reg);
                content_len += snprintf(content + content_len, 1 + 16, "%s\n", reg);
            }
            
            if ((sdd_data_type & 32) && random() % 2) {
                char email[50]; // 假设邮箱地址长度不超过50
                generate_email(email);
                content_len += snprintf(content + content_len, 1+ 50, "%s\n", email);
            }
            
            // left_size代表上文都执行完和要求的数据之间的差距
            size_t left_size = max_content_len - content_len;
            memset(content+content_len, 'A', left_size);
            content_len = max_content_len -1;
            
            // plain
            httpdatalen = snprintf(httpbuf, buflen, HTTP_RSP_FORMAT, content_len + 1, "text/plain", (int) content_len, content);
        }  else {
            /*
             id=123456&bbb=123456
             */
            content_len = sprintf(content, "<html><head><title>test</title></head><body>");
            
            if ((sdd_data_type & 1) && random() % 2) {
                char id[19]; // 18位身份证号码加上结束符
                generate_id(id);
                content_len += snprintf(content + content_len, 19 + 12, "cardNo:%s<br/>", id);
            }
            
            if ((sdd_data_type & 2) && random() % 2) {
                char phone[12]; // 11位手机号码加上结束符
                generate_phone_number(phone);
                content_len += snprintf(content + content_len, 12 + 11, "phone:%s<br/>", phone);
            }
            
            if ((sdd_data_type & 4) && random() % 2) {
                char plate[10]; // 车牌号（1个汉字+1个字母+5个字母或数字+结束符）
                generate_license_plate(plate);
                content_len += snprintf(content + content_len, 10 + 11, "plate:%s<br/>", plate);
            }
            
            if ((sdd_data_type & 8) && random() % 2) {
                char card[17]; // 16位银行卡号加上结束符
                generate_bank_card(card);
                content_len += snprintf(content + content_len, 17 + 10, "card:%s<br/>", card);
            }
            
            if ((sdd_data_type & 16) && random() % 2) {
                char reg[16]; // 15位工商注册号加上结束符
                generate_business_registration(reg);
                content_len += snprintf(content + content_len, 16 + 9, "reg:%s<br/>", reg);
            }
            
            if ((sdd_data_type & 32) && random() % 2) {
                char email[50]; // 假设邮箱地址长度不超过50
                generate_email(email);
                content_len += snprintf(content + content_len, 50 + 11, "email:%s<br/>", email);
            }
            
            // left_size代表上文都执行完和要求的数据之间的差距
            size_t left_size = max_content_len - content_len;
            if (left_size > 15) {
                memset(content+content_len, 'A', max_content_len - content_len - 14);
                content_len = max_content_len-1-14;
            }
            
            
            content_len += sprintf(content + max_content_len - 1 - 14, "</body></html>");
            
            memset(content + content_len, '\0', max_content_len - content_len);
            // plain
            httpdatalen = snprintf(httpbuf, buflen, HTTP_RSP_FORMAT, content_len + 1, "text/html", (int) content_len, content);
        }
                
        //        if (!strncmp(buf, "GET", 3)) {
        io_stat[0]++;
        io_stat[2] += httpdatalen;
        assert(tcp_sndbuf(tpcb) >= httpdatalen);
        assert(tcp_write(tpcb, httpbuf, httpdatalen, TCP_WRITE_FLAG_COPY) == ERR_OK);
        assert(tcp_output(tpcb) == ERR_OK);
        //        }
    } else { /* client mode */
        struct http_response *r = (struct http_response *) arg;
        assert(p->tot_len < (sizeof(r->buf) - r->cur));
        pbuf_copy_partial(p, &r->buf[r->cur], p->tot_len, 0);
        r->cur += p->tot_len;
        //        r->buf[r->cur] = '\0';
        switch (r->state) {
            case 0:
            {
                long i;
                for (i = 0; i < r->cur && r->state == 0; i++) {
                    if (r->buf[i] == 'C') {
                        if (r->cur - i > 15) {
                            if (!memcmp(&r->buf[i], "Content-Length:", 15)) {
                                long j;
                                for (j = 0; j < (r->cur - i - 15); j++) {
                                    if (r->buf[i + 15 + j] == '\r') {
                                        r->buf[i + 15 + j] = '\0';
                                        assert(sscanf(&r->buf[i], "Content-Length: %ld", &r->content_tot_len) == 1);
                                        r->buf[i + 15 + j] = '\r';
                                        r->state = 1;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
                /* fall through */
            case 1:
            {
                long i;
                for (i = 0; i <= r->cur - 4 && r->state == 1; i++) {
                    if (r->buf[i + 0] == '\r' && r->buf[i + 1] == '\n' && r->buf[i + 2] == '\r' && r->buf[i + 3] == '\n') {
                        r->cur -= i + 4;
                        r->content_recvd = 0;
                        r->state = 2;
                        break;
                    }
                }
            }
                /* fall through */
            case 2:
                r->content_recvd += r->cur;
                if (r->content_recvd == r->content_tot_len) {
                    io_stat[0]++;
                    io_stat[2] += httpdatalen;
                    assert(tcp_sndbuf(tpcb) >= httpdatalen);
                    
                    int choice = rand() % 4;
                    // 0/1 no close 2 close 3 force close
                    if (choice > 1) {
                        if (choice == 2) {
                            tcp_close(tpcb);
                        } else if (choice == 3) {
                            tcp_abort(tpcb);
                        }
                        num_stat--;
                        free(r);
                        break;
                    }

                    /*
                     'GET /' is 5 letters
                     如果第五个字符不是`, 就不随机替换
                     */
                    for (i=5; i<random_url+5; i++) {
                        httpbuf[i] = charset[rand() % (sizeof(charset) - 1)];
                    }
                    
                    assert(tcp_write(tpcb, httpbuf, httpdatalen, TCP_WRITE_FLAG_COPY) == ERR_OK);
                    assert(tcp_output(tpcb) == ERR_OK);
                    r->state = 0;
                }
                r->cur = 0;
                break;
            default:
                assert(0);
                break;
        }
    }
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static void tcp_destroy_handeler(unsigned char id __attribute__((unused)), void *data)
{
    if (data)
        free(data);
}

static const struct tcp_ext_arg_callbacks tcp_ext_arg_cbs =  {
    .destroy = tcp_destroy_handeler,
};

static err_t accept_handler(void *arg __attribute__((unused)), struct tcp_pcb *tpcb, err_t err)
{
    if (err != ERR_OK)
        return err;
    
    tcp_recv(tpcb, tcp_recv_handler);
    tcp_setprio(tpcb, TCP_PRIO_MAX);
    
    tcp_ext_arg_set_callbacks(tpcb, 0, &tcp_ext_arg_cbs);
    tcp_ext_arg_set(tpcb, 0, NULL);
    
    tcp_nagle_disable(tpcb);
    
    tpcb->so_options |= SOF_KEEPALIVE;
    tpcb->keep_intvl = (60 * 1000);
    tpcb->keep_idle = (60 * 1000);
    tpcb->keep_cnt = 1;
    
    return err;
}

static err_t connected_handler(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    int i=0;
    if (err != ERR_OK)
        return err;
    if ((err = accept_handler(arg, tpcb, err)) != ERR_OK)
        return err;
    io_stat[2] += httpdatalen;
    assert(tcp_sndbuf(tpcb) >= httpdatalen);
    
    for (i=5; i<random_url+5; i++) {
        httpbuf[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    
    assert(tcp_write(tpcb, httpbuf, httpdatalen, TCP_WRITE_FLAG_COPY) == ERR_OK);
    assert(tcp_output(tpcb) == ERR_OK);
    
    return ERR_OK;
}

static err_t if_init(struct netif *netif)
{
    {
        struct rte_ether_addr ports_eth_addr;
        assert(rte_eth_macaddr_get(0 /* port id */, &ports_eth_addr) >= 0);
        for (int i = 0; i < 6; i++)
            netif->hwaddr[i] = ports_eth_addr.addr_bytes[i];
    }
    {
        uint16_t _mtu;
        assert(rte_eth_dev_get_mtu(0 /* port id */, &_mtu) >= 0);
        assert(_mtu <= PACKET_BUF_SIZE);
        netif->mtu = _mtu;
    }
    netif->output = etharp_output;
    netif->linkoutput = low_level_output;
    netif->hwaddr_len = 6;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_IGMP;
    return ERR_OK;
}

int main(int argc, char *const *argv)
{
    struct netif _netif = { 0 };
    ip4_addr_t _addr, _mask, _gate, _srv_ip;
    int server_port = 10000, num_conn = 1;
    bool mode_server = true;
    int max_epoll_wait_timeout_ms = 0;
    char *url_value = "/";
    char *host_value = "X";
    
    {
        int ret;
        assert((ret = rte_eal_init(argc, (char **) argv)) >= 0);
        argc -= ret;
        argv += ret;
    }
    
    assert(rte_eth_dev_count_avail() == 1);
    
    {
        int ch;
        int i;
        bool _a = false, _g = false, _m = false;
        while ((ch = getopt(argc, argv, "a:c:e:g:l:m:p:s:u:h:r:l:d:")) != -1) {
            switch (ch) {
                case 'a':
                    inet_pton(AF_INET, optarg, &_addr);
                    _a = true;
                    break;
                case 'c':
                    num_conn = atoi(optarg);
                    break;
                case 'd':
                    sdd_data_type = atoi(optarg);
                    break;
                case 'e':
                    assert(sscanf(optarg, "%d", &max_epoll_wait_timeout_ms) == 1);
                    break;
                case 'g':
                    inet_pton(AF_INET, optarg, &_gate);
                    _g = true;
                    break;
                case 'm':
                    inet_pton(AF_INET, optarg, &_mask);
                    _m = true;
                    break;
                case 'r':
                    response_type = atoi(optarg);
                    break;
                case 'l':
                    max_content_len = atoi(optarg);
                    break;
                case 'p':
                    server_port = atoi(optarg);
                    break;
                case 's':
                    inet_pton(AF_INET, optarg, &_srv_ip);
                    mode_server = false;
                    break;
                case 'u':
                    url_value = optarg;
                    for (i=1; ;i++) {
                        if (url_value[i] != '`') {
                            break;
                        }
                        
                        random_url ++;
                    }
                    break;
                case 'h':
                    host_value = optarg;
                    break;
                default:
                    assert(0);
                    break;
            }
        }
        assert(_a && _g && _m);
    }
    
    {
        uint16_t nb_rxd = NUM_SLOT;
        uint16_t nb_txd = NUM_SLOT;
        assert((pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool",
                                                       RTE_MAX(1 /* nb_ports */ * (nb_rxd + nb_txd + MAX_PKT_BURST + 1 * MEMPOOL_CACHE_SIZE), 8192),
                                                       MEMPOOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
                                                       rte_socket_id())) != NULL);
        
        {
            struct rte_eth_dev_info dev_info;
            struct rte_eth_conf local_port_conf = { 0 };
            
            assert(rte_eth_dev_info_get(0 /* port id */, &dev_info) >= 0);
            
            if (max_epoll_wait_timeout_ms)
                local_port_conf.intr_conf.rxq = 1;
            
            assert(rte_eth_dev_configure(0 /* port id */, 1 /* num queues */, 1 /* num queues */, &local_port_conf) >= 0);
            
            assert(rte_eth_dev_adjust_nb_rx_tx_desc(0 /* port id */, &nb_rxd, &nb_txd) >= 0);
            
            assert(rte_eth_rx_queue_setup(0 /* port id */, 0 /* queue */, nb_rxd,
                                          rte_eth_dev_socket_id(0 /* port id */),
                                          &dev_info.default_rxconf,
                                          pktmbuf_pool) >= 0);
            
            assert(rte_eth_tx_queue_setup(0 /* port id */, 0 /* queue */, nb_txd,
                                          rte_eth_dev_socket_id(0 /* port id */),
                                          &dev_info.default_txconf) >= 0);
            
            assert(rte_eth_dev_start(0 /* port id */) >= 0);
            assert(rte_eth_promiscuous_enable(0 /* port id */) >= 0);
            
            if (max_epoll_wait_timeout_ms)
                assert(!rte_eth_dev_rx_intr_ctl_q(0 /* port id */, 0 /* queue */, RTE_EPOLL_PER_THREAD, RTE_INTR_EVENT_ADD, NULL));
        }
    }
    
    /* setting up lwip */
    {
        lwip_init();
        assert(netif_add(&_netif, &_addr, &_mask, &_gate, NULL, if_init, ethernet_input) != NULL);
        netif_set_default(&_netif);
        netif_set_link_up(&_netif);
        netif_set_up(&_netif);
    }
    
    if (mode_server) { /* server mode */
        {
            // FIXME: 256 now is right for req hdr but if feature, change it self!
            assert((content = (char *) malloc(max_content_len)) != NULL);
            assert((httpbuf = (char *) malloc(256 + max_content_len + 1)) != NULL);
        }
        {
            struct tcp_pcb *tpcb, *_tpcb;
            assert((_tpcb = tcp_new()) != NULL);
            assert(tcp_bind(_tpcb, IP_ADDR_ANY, server_port) == ERR_OK);
            assert((tpcb = tcp_listen(_tpcb)) != NULL);
            tcp_accept(tpcb, accept_handler);
            tcp_ext_arg_set_callbacks(tpcb, 0, &tcp_ext_arg_cbs);
            tcp_ext_arg_set(tpcb, 0, NULL);
        }
    } else { /* client mode */
        {
            size_t urllen = strlen(url_value);
            size_t hostlen = strlen(host_value);
            size_t buflen = hostlen + urllen + 50 /* for url(max 128) + http base */;
            assert((httpbuf = (char *) malloc(buflen)) != NULL);
            httpdatalen = snprintf(httpbuf, buflen, HTTP_REQ_FORMAT, url_value, host_value);
            printf("http data length: %lu bytes\n", httpdatalen);
            printf("http data : %s\n", httpbuf);
        }
        int i;
        printf("%d concurrent connection(s)\n", num_conn);
        for (i = 0; i < num_conn; i++) {
            struct tcp_pcb *tpcb;
            assert((tpcb = tcp_new()) != NULL);
            {
                struct http_response *r;
                assert((r = (struct http_response *) malloc(sizeof(struct http_response))) != NULL);
                r->state = 0;
                r->cur = 0;
                tcp_arg(tpcb, (void *) r);
                tcp_ext_arg_set(tpcb, 0, (void *) r);
            }
            assert(tcp_connect(tpcb, &_srv_ip, server_port, connected_handler) == ERR_OK);
            
            num_stat += 1;
        }
    }
    
    printf("-- application has started --\n");
    
    /* primary loop */
    {
        unsigned long prev_ts = 0;
        while (1) {
            if (mode_server) {
                
            } else {
//                printf("%d concurrent2 connection(s)\n\n", num_stat);
                for (; num_stat < num_conn; num_stat++) {
                    struct tcp_pcb *tpcb;
                    assert((tpcb = tcp_new()) != NULL);
                    {
                        struct http_response *r;
                        assert((r = (struct http_response *) malloc(sizeof(struct http_response))) != NULL);
                        r->state = 0;
                        r->cur = 0;
                        tcp_arg(tpcb, (void *) r);
                        tcp_ext_arg_set(tpcb, 0, (void *) r);
                    }
                    assert(tcp_connect(tpcb, &_srv_ip, server_port, connected_handler) == ERR_OK);
//                    printf("%d concurrent4 connection(s)\n\n", num_stat);
                }
//                printf("%d concurrent3 connection(s)\n\n", num_stat);
            }
            
            struct rte_mbuf *rx_mbufs[MAX_PKT_BURST];
            unsigned short i, nb_rx = rte_eth_rx_burst(0 /* port id */, 0 /* queue id */, rx_mbufs, MAX_PKT_BURST);
            for (i = 0; i < nb_rx; i++) {
                {
                    struct pbuf *p;
                    assert((p = pbuf_alloc(PBUF_RAW, rte_pktmbuf_pkt_len(rx_mbufs[i]), PBUF_POOL)) != NULL);
                    pbuf_take(p, rte_pktmbuf_mtod(rx_mbufs[i], void *), rte_pktmbuf_pkt_len(rx_mbufs[i]));
                    p->len = p->tot_len = rte_pktmbuf_pkt_len(rx_mbufs[i]);
                    assert(_netif.input(p, &_netif) == ERR_OK);
                }
                rte_pktmbuf_free(rx_mbufs[i]);
            }
            tx_flush();
            sys_check_timeouts();
            {
                unsigned long now = ({ struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); (ts.tv_sec * 1000000000UL + ts.tv_nsec); });
                if (now - prev_ts > 1000000000UL) {
                    if (io_stat[1] > 1000000 || io_stat[2] > 1000000) {
                        printf("[%s]: %10lu Requests/sec rx %5lu Mbps, tx %5lu Mbps, total %5lu Mbps\n",
                               (mode_server ? "server" : "client"), io_stat[0], io_stat[1] * 8 / 1000000, io_stat[2] * 8 / 1000000, (io_stat[1] * 8 + io_stat[2] * 8) / 1000000);
                    } else {
                        printf("[%s]: %10lu Requests/sec  (rx %11lu bps, tx %11lu bps)\n",
                               (mode_server ? "server" : "client"), io_stat[0], io_stat[1] * 8, io_stat[2] * 8);
                    }
                    memset(io_stat, 0, sizeof(io_stat));
                    prev_ts = now;
                }
                
            }
            if (!nb_rx && max_epoll_wait_timeout_ms) {
                assert(!rte_eth_dev_rx_intr_enable(0 /* port id */, 0 /* queue id */));
                {
                    struct rte_epoll_event ev;
                    (void) rte_epoll_wait(RTE_EPOLL_PER_THREAD, &ev, 1, max_epoll_wait_timeout_ms < 0 ? 100 : (max_epoll_wait_timeout_ms > 100 ? 100 : max_epoll_wait_timeout_ms));
                }
                rte_eth_dev_rx_intr_disable(0 /* port id */, 0 /* queue id */);
            }
        }
    }
    
    return 0;
}
