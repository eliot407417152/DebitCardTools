#include "ReaderManager.h"
#include <cstring>

ReaderManager::ReaderManager(QObject* parent)
    : QObject(parent)
{
    // 啟動時先偵測一次（此時還沒有人接 message 訊號，所以是靜默的；
    // 由使用端在接好訊號後自行顯示偵測結果）
    if (establishContext())
        listReaders();
}

ReaderManager::~ReaderManager()
{
    for (auto it = m_handles.begin(); it != m_handles.end(); ++it) {
        if (it.value())
            SCardDisconnect(it.value(), SCARD_LEAVE_CARD);
    }
    m_handles.clear();
    releaseContext();
}

bool ReaderManager::establishContext()
{
    if (m_context)
        return true;

    LONG ret = SCardEstablishContext(SCARD_SCOPE_SYSTEM, nullptr, nullptr, &m_context);
    if (ret != SCARD_S_SUCCESS) {
        m_context = 0;
        // 沒插任何讀卡機時，Windows 的智慧卡服務可能沒有啟動，屬正常現象，插上後按「重新整理」即可
        emit message(QString("⚠ 無法建立 PC/SC 環境（%1），請確認已插上讀卡機後按「重新整理」")
            .arg(errorText(ret)), true);
        return false;
    }
    return true;
}

void ReaderManager::releaseContext()
{
    if (m_context) {
        SCardReleaseContext(m_context);
        m_context = 0;
    }
}

void ReaderManager::listReaders()
{
    m_readers.clear();
    if (!m_context)
        return;

    DWORD dwLen = SCARD_AUTOALLOCATE;
    LPSTR mszReaders = nullptr;
    LONG ret = SCardListReadersA(m_context, nullptr,
        reinterpret_cast<LPSTR>(&mszReaders), &dwLen);

    if (ret == SCARD_E_NO_READERS_AVAILABLE)
        return;   // 沒有讀卡機：清單維持空白

    if (ret != SCARD_S_SUCCESS) {
        emit message(QString("⚠ 偵測讀卡機失敗（%1）").arg(errorText(ret)), true);
        return;
    }

    for (LPSTR p = mszReaders; *p != '\0'; p += strlen(p) + 1)
        m_readers.append(QString::fromLocal8Bit(p));
    SCardFreeMemory(m_context, mszReaders);
}

bool ReaderManager::isConnected(const QString& readerName) const
{
    auto it = m_handles.constFind(readerName);
    return it != m_handles.constEnd() && it.value() != 0;
}

// 用 SCardStatus 確認連線代碼是否還有效（卡片被拔掉 / 重插 / 被重置後，舊代碼都會失效）
bool ReaderManager::isHandleAlive(SCARDHANDLE h) const
{
    if (!h)
        return false;
    DWORD state = 0, proto = 0;
    LONG ret = SCardStatusA(h, nullptr, nullptr, &state, &proto, nullptr, nullptr);
    return ret == SCARD_S_SUCCESS;
}

bool ReaderManager::doConnect(const QString& readerName)
{
    if (!establishContext())
        return false;

    DWORD proto = 0;
    SCARDHANDLE h = 0;
    LONG ret = SCardConnectA(m_context,
        readerName.toLocal8Bit().constData(),
        SCARD_SHARE_SHARED,
        SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
        &h, &proto);

    if (ret != SCARD_S_SUCCESS) {
        emit message(QString("SCardConnect [%1] 失敗：%2").arg(readerName, errorText(ret)), true);
        emit connectionChanged(readerName);
        return false;
    }

    m_handles.insert(readerName, h);
    emit message(QString("✔ 已連線讀卡機：%1").arg(readerName), false);
    emit connectionChanged(readerName);
    return true;
}

bool ReaderManager::connectReader(const QString& readerName)
{
    if (readerName.isEmpty()) {
        emit message("Error: 尚未選擇讀卡機", true);
        return false;
    }

    auto it = m_handles.find(readerName);
    if (it != m_handles.end()) {
        if (isHandleAlive(it.value()))
            return true;
        // 舊連線已失效：先放掉再重連
        SCardDisconnect(it.value(), SCARD_LEAVE_CARD);
        m_handles.erase(it);
    }
    return doConnect(readerName);
}

SCARDHANDLE ReaderManager::handle(const QString& readerName)
{
    if (readerName.isEmpty()) {
        emit message("Error: 尚未選擇讀卡機", true);
        return 0;
    }

    auto it = m_handles.find(readerName);
    if (it != m_handles.end()) {
        if (isHandleAlive(it.value()))
            return it.value();

        emit message(QString("⚠ 讀卡機 [%1] 的連線已失效（卡片可能被拔出或重插），自動重新連線…")
            .arg(readerName), false);
        SCardDisconnect(it.value(), SCARD_LEAVE_CARD);
        m_handles.erase(it);
        emit connectionChanged(readerName);
    }

    return doConnect(readerName) ? m_handles.value(readerName, 0) : 0;
}

void ReaderManager::disconnectReader(const QString& readerName)
{
    auto it = m_handles.find(readerName);
    if (it == m_handles.end())
        return;

    if (it.value())
        SCardDisconnect(it.value(), SCARD_LEAVE_CARD);
    m_handles.erase(it);
    emit message(QString("✔ 已斷線讀卡機：%1").arg(readerName), false);
    emit connectionChanged(readerName);
}

void ReaderManager::disconnectAll()
{
    const QStringList names = m_handles.keys();
    for (const QString& name : names) {
        SCARDHANDLE h = m_handles.take(name);
        if (h)
            SCardDisconnect(h, SCARD_LEAVE_CARD);
        emit connectionChanged(name);
    }
}

bool ReaderManager::refresh()
{
    // 記下重新整理前已連線的讀卡機，整理完再幫忙連回去
    const QStringList wasConnected = m_handles.keys();

    disconnectAll();
    releaseContext();

    if (establishContext())
        listReaders();

    emit readersChanged();

    if (m_readers.isEmpty())
        emit message("⚠ 重新整理完成：找不到任何讀卡機", true);
    else
        emit message(QString("✔ 重新整理完成：偵測到 %1 台讀卡機（%2）")
            .arg(m_readers.size()).arg(m_readers.join("、")), false);

    for (const QString& name : wasConnected) {
        if (m_readers.contains(name))
            doConnect(name);
    }

    return !m_readers.isEmpty();
}

QString ReaderManager::errorText(LONG code)
{
    const quint32 u = static_cast<quint32>(code);
    QString text;
    switch (u) {
    case 0x8010000C: text = "讀卡機內沒有卡片"; break;                 // SCARD_E_NO_SMARTCARD
    case 0x8010001D: text = "智慧卡服務未啟動"; break;                 // SCARD_E_NO_SERVICE
    case 0x8010001E: text = "智慧卡服務已停止"; break;                 // SCARD_E_SERVICE_STOPPED
    case 0x8010000B: text = "讀卡機被其他程式佔用"; break;             // SCARD_E_SHARING_VIOLATION
    case 0x80100017: text = "讀卡機無法使用（可能已拔除）"; break;     // SCARD_E_READER_UNAVAILABLE
    case 0x80100069: text = "卡片已被移除"; break;                     // SCARD_W_REMOVED_CARD
    case 0x80100068: text = "卡片已被重置"; break;                     // SCARD_W_RESET_CARD
    case 0x80100066: text = "無法讀取卡片（可能插反或接觸不良）"; break; // SCARD_W_UNRESPONSIVE_CARD
    case 0x80100065: text = "不支援此卡片"; break;                     // SCARD_W_UNSUPPORTED_CARD
    default: break;
    }
    const QString hex = "0x" + QString::number(u, 16).rightJustified(8, QChar('0')).toUpper();
    return text.isEmpty() ? hex : QString("%1，%2").arg(text, hex);
}
