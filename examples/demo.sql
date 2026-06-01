CREATE DATABASE shop;
USE shop;

CREATE TABLE products(
    id INT NOT_NULL INDEXED DEFAULT 0,
    name STRING NOT_NULL,
    price INT DEFAULT 0
);

INSERT
    INTO products (id, name, price)
    VALUE (1, "Apple", 99),
          (2, "Banana", 75);

SELECT
    AVG(price) AS avg_price
FROM
    products
WHERE
    price >= 1 AND name LIKE "^A.*";
