#include "../../include/ssl/SslConfig.h"

namespace ssl
{
SslConfig::SslConfig()
    : version_(SSLVersion::TLS_1_2)
    , certFile_("HIGH:aNULL:!MDS") // HIGH仅启用高强度的加密算法
    // aNULL禁用无证书的加密套件
    //禁用MDS算法，已知不安全易受哈希碰撞威胁
    , verifyClient_(false)
    , verifyDepth_(4)
    , sessionTimeout_(300)
    , sessionCacheSize_(20480L) //( 20 * 1024 = 20KB)
{

}
} // namespace ssl
