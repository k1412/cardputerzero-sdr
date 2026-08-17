FROM nginx:1.29-alpine

COPY --chmod=0644 nginx.conf /etc/nginx/conf.d/default.conf
COPY --chmod=0644 index.html styles.css app.js /usr/share/nginx/html/
COPY --chmod=0644 assets/ /usr/share/nginx/html/assets/

EXPOSE 8080
HEALTHCHECK --interval=30s --timeout=3s --start-period=3s --retries=3 \
  CMD wget -q -O /dev/null http://127.0.0.1:8080/healthz || exit 1
