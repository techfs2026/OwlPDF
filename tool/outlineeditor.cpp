#include "outlineeditor.h"
#include "outlineitem.h"
#include "perthreadmupdfrenderer.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>
#include <QMetaObject>
#include <QThread>
#include <QCoreApplication>

OutlineEditor::OutlineEditor(PerThreadMuPDFRenderer* renderer, QObject* parent)
    : QObject(parent)
    , m_renderer(renderer)
    , m_root(nullptr)
    , m_modified(false)
{
}

OutlineEditor::~OutlineEditor()
{
}

void OutlineEditor::setRoot(OutlineItem* root)
{
    if (!root) {
        qWarning() << "OutlineEditor::setRoot: root is nullptr, creating virtual root";
        m_root = new OutlineItem();
    } else {
        m_root = root;
    }

    m_modified = false;
}

OutlineItem* OutlineEditor::addOutline(OutlineItem* parentItem,
                                       const QString& title,
                                       int pageIndex,
                                       int insertIndex)
{
    if (!m_renderer || !m_renderer->isDocumentLoaded()) {
        qWarning() << "OutlineEditor: No document loaded";
        return nullptr;
    }

    if (!validateOutline(title, pageIndex)) {
        qWarning() << "OutlineEditor: Invalid outline parameters";
        return nullptr;
    }

    if (!m_root) {
        qWarning() << "OutlineEditor: No root, creating virtual root";
        m_root = new OutlineItem();
    }

    OutlineItem* parent = parentItem ? parentItem : m_root;
    if (!parent) {
        qWarning() << "OutlineEditor: No valid parent node";
        return nullptr;
    }

    OutlineItem* newItem = new OutlineItem(title, pageIndex);

    if (insertIndex >= 0 && insertIndex < parent->childCount()) {
        parent->addChild(newItem);
    } else {
        parent->addChild(newItem);
    }

    m_modified = true;
    emit outlineModified();

    qInfo() << "OutlineEditor: Added outline:" << title << "at page" << (pageIndex + 1);
    return newItem;
}

bool OutlineEditor::deleteOutline(OutlineItem* item)
{
    if (!item || !m_root) return false;
    if (item == m_root) {
        qWarning() << "OutlineEditor: Cannot delete root node";
        return false;
    }

    OutlineItem* parent = item->parent();
    if (!parent) {
        qWarning() << "OutlineEditor: Item has no parent";
        return false;
    }

    bool removed = parent->removeChild(item);
    if (!removed) {
        qWarning() << "OutlineEditor: Failed to remove item from parent";
        return false;
    }

    delete item;

    m_modified = true;
    emit outlineModified();
    qInfo() << "OutlineEditor: Deleted outline";
    return true;
}

bool OutlineEditor::deleteAllOutlines()
{
    if (!m_root) {
        qWarning() << "OutlineEditor::deleteAllOutlines: No root available";
        return false;
    }

    if (m_root->childCount() == 0) {
        qInfo() << "OutlineEditor::deleteAllOutlines: Already empty";
        return true;
    }

    qInfo() << "OutlineEditor::deleteAllOutlines: Deleting"
            << m_root->childCount() << "root items";

    while (m_root->childCount() > 0) {
        OutlineItem* child = m_root->child(0);
        m_root->removeChild(child);
        delete child;
    }

    m_modified = true;
    emit outlineModified();

    qInfo() << "OutlineEditor::deleteAllOutlines: All outlines deleted";

    return true;
}

bool OutlineEditor::renameOutline(OutlineItem* item, const QString& newTitle)
{
    if (!item || newTitle.isEmpty()) return false;
    if (item == m_root) {
        qWarning() << "OutlineEditor: Cannot rename root node";
        return false;
    }

    QString oldTitle = item->title();
    item->setTitle(newTitle);

    m_modified = true;
    emit outlineModified();

    qInfo() << "OutlineEditor: Renamed outline from" << oldTitle << "to" << newTitle;
    return true;
}

bool OutlineEditor::updatePageIndex(OutlineItem* item, int newPageIndex)
{
    if (!item) return false;

    if (item == m_root) {
        qWarning() << "OutlineEditor: Cannot update root node page index";
        return false;
    }

    if (newPageIndex < -1) {
        qWarning() << "OutlineEditor: Invalid page index:" << newPageIndex;
        return false;
    }

    if (m_renderer && newPageIndex >= m_renderer->pageCount()) {
        qWarning() << "OutlineEditor: Page index out of range:" << newPageIndex;
        return false;
    }

    int oldPageIndex = item->pageIndex();

    if (oldPageIndex == newPageIndex) {
        return true;
    }

    item->setPageIndex(newPageIndex);

    m_modified = true;
    emit outlineModified();

    qInfo() << "OutlineEditor: Updated page index from" << (oldPageIndex + 1)
            << "to" << (newPageIndex + 1);

    return true;
}

bool OutlineEditor::moveOutline(OutlineItem* item,
                                OutlineItem* newParent,
                                int newIndex)
{
    if (!item || !m_root)
        return false;

    if (item == m_root) {
        qWarning() << "OutlineEditor: Cannot move root node";
        return false;
    }

    OutlineItem* targetParent = newParent ? newParent : m_root;

    OutlineItem* p = targetParent;
    while (p) {
        if (p == item) {
            qWarning() << "OutlineEditor: Cannot move to descendant";
            return false;
        }
        p = p->parent();
    }

    OutlineItem* oldParent = item->parent();
    if (!oldParent) {
        qWarning() << "OutlineEditor: Item has no parent";
        return false;
    }

    oldParent->removeChild(item);

    if (newIndex < 0 || newIndex > targetParent->childCount())
        newIndex = targetParent->childCount();

    targetParent->insertChild(newIndex, item);

    m_modified = true;
    emit outlineModified();

    qInfo() << "OutlineEditor: Moved outline successfully";
    return true;
}

namespace {
inline pdf_obj* create_doc_dict(fz_context* ctx, pdf_document* doc, int initial = 4)
{
    pdf_obj* dict = nullptr;
    fz_try(ctx) {
        dict = pdf_add_object(ctx, doc, pdf_new_dict(ctx, doc, initial));
    }
    fz_catch(ctx) {
        qWarning() << "create_doc_dict: error creating dict:" << fz_caught_message(ctx);
        return nullptr;
    }
    return dict;
}

bool validate_tree(OutlineItem* root, PerThreadMuPDFRenderer* renderer, QString* reason)
{
    if (!root) {
        if (reason) *reason = QCoreApplication::translate("OutlineEditor", "Root node is null");
        return false;
    }
    if (!renderer || !renderer->isDocumentLoaded()) {
        if (reason) *reason = QCoreApplication::translate("OutlineEditor", "No document loaded");
        return false;
    }

    int pageCount = renderer->pageCount();
    QList<OutlineItem*> stack;
    stack.append(root);

    while (!stack.isEmpty()) {
        OutlineItem* item = stack.takeLast();
        if (!item) continue;

        if (item != root) {
            if (item->title().trimmed().isEmpty()) {
                if (reason) *reason = QCoreApplication::translate("OutlineEditor", "Empty title found");
                return false;
            }
            int pg = item->pageIndex();
            if (pg < -1 || pg >= pageCount) {
                if (reason) *reason = QCoreApplication::translate("OutlineEditor", "Page index out of range: %1").arg(pg);
                return false;
            }
        }

        for (int i = item->childCount() - 1; i >= 0; --i) {
            OutlineItem* child = item->child(i);
            if (child) stack.append(child);
        }
    }

    return true;
}

pdf_obj* buildPdfOutlineRecursive(fz_context* ctx, pdf_document* doc,
                                  PerThreadMuPDFRenderer* renderer, OutlineItem* item)
{
    if (!ctx || !doc || !item || !renderer) return nullptr;

    pdf_obj* item_obj = nullptr;
    fz_try(ctx) {
        item_obj = create_doc_dict(ctx, doc, 8);
        if (!item_obj) fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to create item dict");

        pdf_dict_put(ctx, item_obj, PDF_NAME(Title), pdf_new_text_string(ctx, item->title().toUtf8().constData()));

        int pg = item->pageIndex();
        if (pg >= 0 && pg < renderer->pageCount()) {
            pdf_obj* pageObj = pdf_lookup_page_obj(ctx, doc, pg);
            if (pageObj) {
                pdf_obj* dest = pdf_new_array(ctx, doc, 5);
                pdf_array_push(ctx, dest, pageObj);
                pdf_array_push(ctx, dest, PDF_NAME(XYZ));
                pdf_array_push_real(ctx, dest, 0);
                pdf_array_push_real(ctx, dest, 0);
                pdf_array_push_real(ctx, dest, 0);

                pdf_dict_put(ctx, item_obj, PDF_NAME(Dest), dest);
                pdf_drop_obj(ctx, dest);
            }
        }

        QList<pdf_obj*> children;
        for (int i = 0; i < item->childCount(); ++i) {
            OutlineItem* child = item->child(i);
            if (!child || !child->isValid()) continue;

            pdf_obj* co = buildPdfOutlineRecursive(ctx, doc, renderer, child);
            if (!co) continue;

            pdf_dict_put(ctx, co, PDF_NAME(Parent), item_obj);
            children.append(co);
        }

        if (!children.isEmpty()) {
            pdf_dict_put(ctx, item_obj, PDF_NAME(First), children.first());
            pdf_dict_put(ctx, item_obj, PDF_NAME(Last), children.last());
            pdf_dict_put_int(ctx, item_obj, PDF_NAME(Count), children.size());

            for (int i = 0; i < children.size(); ++i) {
                if (i > 0) pdf_dict_put(ctx, children[i], PDF_NAME(Prev), children[i - 1]);
                if (i < children.size() - 1) pdf_dict_put(ctx, children[i], PDF_NAME(Next), children[i + 1]);
            }

            for (pdf_obj* co : children) pdf_drop_obj(ctx, co);
        }
    }
    fz_catch(ctx) {
        qWarning() << "buildPdfOutlineRecursive: caught mu error:" << fz_caught_message(ctx);
        if (item_obj) pdf_drop_obj(ctx, item_obj);
        return nullptr;
    }

    return item_obj;
}
}

bool OutlineEditor::saveToDocument(const QString& filePath)
{
    if (!m_renderer || !m_renderer->isDocumentLoaded()) {
        QString msg = QCoreApplication::translate("OutlineEditor", "No document loaded");
        qWarning() << "OutlineEditor:" << msg;
        emit saveCompleted(false, msg);
        return false;
    }
    if (!m_root) {
        QString msg = QCoreApplication::translate("OutlineEditor", "No outline data");
        qWarning() << "OutlineEditor:" << msg;
        emit saveCompleted(false, msg);
        return false;
    }

    QString reason;
    if (!validate_tree(m_root, m_renderer, &reason)) {
        qWarning() << "OutlineEditor: invalid outline tree:" << reason;
        emit saveCompleted(false, reason);
        return false;
    }

    fz_context* ctx = static_cast<fz_context*>(m_renderer->context());
    fz_document* fzdoc = static_cast<fz_document*>(m_renderer->document());
    if (!ctx || !fzdoc) {
        QString msg = QCoreApplication::translate("OutlineEditor", "Invalid MuPDF context or document");
        qWarning() << "OutlineEditor:" << msg;
        emit saveCompleted(false, msg);
        return false;
    }

    pdf_document* pdfDoc = pdf_document_from_fz_document(ctx, fzdoc);
    if (!pdfDoc) {
        QString msg = QCoreApplication::translate("OutlineEditor", "Document is not a PDF");
        qWarning() << "OutlineEditor:" << msg;
        emit saveCompleted(false, msg);
        return false;
    }

    QString savePath = filePath.isEmpty() ? m_renderer->documentPath() : filePath;
    if (savePath.isEmpty()) {
        QString msg = QCoreApplication::translate("OutlineEditor", "No file path specified");
        qWarning() << "OutlineEditor:" << msg;
        emit saveCompleted(false, msg);
        return false;
    }

    QString backupPath = createBackup(savePath);
    if (!backupPath.isEmpty()) qInfo() << "OutlineEditor: Backup created at:" << backupPath;

    bool success = false;
    QString errorMsg;

    fz_try(ctx) {
        pdf_obj* trailer = pdf_trailer(ctx, pdfDoc);
        pdf_obj* catalog_ref = pdf_dict_get(ctx, trailer, PDF_NAME(Root));
        pdf_obj* catalog = nullptr;
        if (catalog_ref) catalog = pdf_resolve_indirect(ctx, catalog_ref);

        if (!catalog || !pdf_is_dict(ctx, catalog)) {
            fz_throw(ctx, FZ_ERROR_GENERIC, "Invalid PDF catalog");
        }

        pdf_dict_del(ctx, catalog, PDF_NAME(Outlines));

        if (m_root->childCount() > 0) {
            pdf_obj* outlines = create_doc_dict(ctx, pdfDoc, 4);
            if (!outlines) fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to create outlines dict");

            fz_try(ctx) {
                pdf_dict_put(ctx, outlines, PDF_NAME(Type), PDF_NAME(Outlines));

                QList<pdf_obj*> topItems;
                for (int i = 0; i < m_root->childCount(); ++i) {
                    OutlineItem* child = m_root->child(i);
                    if (!child || !child->isValid()) continue;

                    pdf_obj* item_obj = buildPdfOutlineRecursive(ctx, pdfDoc, m_renderer, child);
                    if (!item_obj) {
                        qWarning() << "OutlineEditor: buildPdfOutlineRecursive returned null for top child" << i;
                        continue;
                    }

                    pdf_dict_put(ctx, item_obj, PDF_NAME(Parent), outlines);
                    topItems.append(item_obj);
                }

                if (!topItems.isEmpty()) {
                    pdf_dict_put(ctx, outlines, PDF_NAME(First), topItems.first());
                    pdf_dict_put(ctx, outlines, PDF_NAME(Last),  topItems.last());
                    pdf_dict_put_int(ctx, outlines, PDF_NAME(Count), topItems.size());

                    for (int i = 0; i < topItems.size(); ++i) {
                        if (i > 0) pdf_dict_put(ctx, topItems[i], PDF_NAME(Prev), topItems[i-1]);
                        if (i < topItems.size() - 1) pdf_dict_put(ctx, topItems[i], PDF_NAME(Next), topItems[i+1]);
                    }

                    for (pdf_obj* obj : topItems) pdf_drop_obj(ctx, obj);
                }

                pdf_dict_put(ctx, catalog, PDF_NAME(Outlines), outlines);
            }
            fz_always(ctx) {
                pdf_drop_obj(ctx, outlines);
            }
            fz_catch(ctx) {
                fz_rethrow(ctx);
            }
        }

        pdf_write_options opts = pdf_default_write_options;
        opts.do_incremental = 1;
        opts.do_garbage = 0;

        QByteArray pathBytes = savePath.toUtf8();
        qInfo() << "OutlineEditor: about to save PDF to" << savePath;
        pdf_save_document(ctx, pdfDoc, pathBytes.constData(), &opts);

        success = true;
        m_modified = false;
        qInfo() << "OutlineEditor: saveToDocument completed successfully";
    }
    fz_catch(ctx) {
        errorMsg = QString::fromUtf8(fz_caught_message(ctx));
        qWarning() << "OutlineEditor: saveToDocument failed:" << errorMsg;
        success = false;
    }

    emit saveCompleted(success, errorMsg);
    return success;
}

void* OutlineEditor::createPdfOutline(void* ctx_ptr, void* doc_ptr, OutlineItem* item)
{
    if (!ctx_ptr || !doc_ptr || !item) return nullptr;
    fz_context* ctx = static_cast<fz_context*>(ctx_ptr);
    pdf_document* doc = static_cast<pdf_document*>(doc_ptr);

    pdf_obj* obj = buildPdfOutlineRecursive(ctx, doc, m_renderer, item);
    return static_cast<void*>(obj);
}

void* OutlineEditor::buildPdfOutlineTree(void* ctx_ptr, void* doc_ptr,
                                         OutlineItem* item, void* parent_ptr)
{
    return createPdfOutline(ctx_ptr, doc_ptr, item);
}

bool OutlineEditor::validateOutline(const QString& title, int pageIndex) const
{
    if (title.trimmed().isEmpty()) {
        qWarning() << "OutlineEditor: Empty title";
        return false;
    }

    if (pageIndex < -1) {
        qWarning() << "OutlineEditor: Invalid page index:" << pageIndex;
        return false;
    }

    if (m_renderer && pageIndex >= m_renderer->pageCount()) {
        qWarning() << "OutlineEditor: Page index out of range:" << pageIndex;
        return false;
    }

    return true;
}

int OutlineEditor::findItemIndex(OutlineItem* item) const
{
    if (!item || !item->parent()) {
        return -1;
    }

    OutlineItem* parent = item->parent();
    for (int i = 0; i < parent->childCount(); ++i) {
        if (parent->child(i) == item) {
            return i;
        }
    }

    return -1;
}

bool OutlineEditor::removeFromParent(OutlineItem* item)
{
    if (!item) return false;
    OutlineItem* parent = item->parent();
    if (!parent) return false;

    bool removed = parent->removeChild(item);
    if (removed) {
        item->setParent(nullptr);
        return true;
    }

    item->setParent(nullptr);
    return true;
}

QString OutlineEditor::createBackup(const QString& filePath) const
{
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        return QString();
    }

    QFileInfo fileInfo(filePath);
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString backupPath = fileInfo.absolutePath() + "/" +
                         fileInfo.baseName() + "_backup_" + timestamp + "." +
                         fileInfo.completeSuffix();

    if (QFile::copy(filePath, backupPath)) {
        return backupPath;
    }

    return QString();
}
