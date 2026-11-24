#pragma once
#include <QDialog>
#include <vector>
#include "CardPersonal.h"

class QLineEdit;
class QGroupBox;
class QSpinBox;

// 寫入 / 更新個人化資料的輸入視窗:欄位依「資料名稱」分開,一個欄位一個輸入框
class RecordDialog : public QDialog {
public:
	enum class Mode { Write, Update };
	RecordDialog(const personal::FileDef& def, Mode mode, QWidget* parent = nullptr);

	std::vector<CardPersonal::RecordInput> result() const { return result_; }

protected:
	void accept() override;   // 檢查每個欄位長度後才關閉

private:
	struct FieldEdit { QLineEdit* edit; const personal::Field* field; };
	struct RecordBox {
		const personal::Record* rec;
		QGroupBox* box;
		QSpinBox* recNo = nullptr;
		std::vector<FieldEdit> fields;
	};

	Mode mode_;
	std::vector<RecordBox> boxes_;
	std::vector<CardPersonal::RecordInput> result_;
};

// 查看個人化資料:依欄位名稱顯示讀回來的內容
void showRecords(QWidget* parent, const personal::FileDef& def,
	const std::vector<CardPersonal::RecordData>& recs);